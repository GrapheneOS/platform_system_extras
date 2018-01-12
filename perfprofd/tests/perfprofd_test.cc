/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <cctype>
#include <memory>
#include <regex>
#include <string>

#include <fcntl.h>
#include <stdio.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android-base/test_utils.h>
#include <gtest/gtest.h>

#include "config.h"
#include "configreader.h"
#include "perfprofdcore.h"
#include "symbolizer.h"

#include "perf_profile.pb.h"
#include "google/protobuf/text_format.h"

//
// Set to argv[0] on startup
//
static std::string gExecutableRealpath;

namespace {

using android::base::LogId;
using android::base::LogSeverity;

static std::vector<std::string>* gTestLogMessages = nullptr;

static void TestLogFunction(LogId log_id ATTRIBUTE_UNUSED,
                            LogSeverity severity,
                            const char* tag,
                            const char* file ATTRIBUTE_UNUSED,
                            unsigned int line ATTRIBUTE_UNUSED,
                            const char* message) {
  constexpr char log_characters[] = "VDIWEFF";
  char severity_char = log_characters[severity];
  gTestLogMessages->push_back(android::base::StringPrintf("%c: %s", severity_char, message));
}

static void InitTestLog() {
  CHECK(gTestLogMessages == nullptr);
  gTestLogMessages = new std::vector<std::string>();
}
static void ClearTestLog() {
  CHECK(gTestLogMessages != nullptr);
  delete gTestLogMessages;
  gTestLogMessages = nullptr;
}
static std::string JoinTestLog(const char* delimiter) {
  CHECK(gTestLogMessages != nullptr);
  return android::base::Join(*gTestLogMessages, delimiter);
}

}  // namespace

// Path to perf executable on device
#define PERFPATH "/system/bin/perf"

// Temporary config file that we will emit for the daemon to read
#define CONFIGFILE "perfprofd.conf"

class PerfProfdTest : public testing::Test {
 protected:
  virtual void SetUp() {
    InitTestLog();
    android::base::SetLogger(TestLogFunction);
    create_dirs();
  }

  virtual void TearDown() {
    android::base::SetLogger(android::base::StderrLogger);
    ClearTestLog();

    // TODO: proper management of test files. For now, use old system() code.
    for (const auto dir : { &dest_dir, &conf_dir }) {
      std::string cmd("rm -rf ");
      cmd += *dir;
      int ret = system(cmd.c_str());
      CHECK_EQ(0, ret);
    }
  }

 protected:
  // test_dir is the directory containing the test executable and
  // any files associated with the test (will be created by the harness).
  std::string test_dir;

  // dest_dir is a temporary directory that we're using as the destination directory.
  // It is backed by temp_dir1.
  std::string dest_dir;

  // conf_dir is a temporary directory that we're using as the configuration directory.
  // It is backed by temp_dir2.
  std::string conf_dir;

 private:
  void create_dirs() {
    temp_dir1.reset(new TemporaryDir());
    temp_dir2.reset(new TemporaryDir());
    dest_dir = temp_dir1->path;
    conf_dir = temp_dir2->path;
    test_dir = android::base::Dirname(gExecutableRealpath);
  }

  std::unique_ptr<TemporaryDir> temp_dir1;
  std::unique_ptr<TemporaryDir> temp_dir2;
};

static bool bothWhiteSpace(char lhs, char rhs)
{
  return (std::isspace(lhs) && std::isspace(rhs));
}

//
// Squeeze out repeated whitespace from expected/actual logs.
//
static std::string squeezeWhite(const std::string &str,
                                const char *tag,
                                bool dump=false)
{
  if (dump) { fprintf(stderr, "raw %s is %s\n", tag, str.c_str()); }
  std::string result(str);
  std::replace(result.begin(), result.end(), '\n', ' ');
  auto new_end = std::unique(result.begin(), result.end(), bothWhiteSpace);
  result.erase(new_end, result.end());
  while (result.begin() != result.end() && std::isspace(*result.rbegin())) {
    result.pop_back();
  }
  if (dump) { fprintf(stderr, "squeezed %s is %s\n", tag, result.c_str()); }
  return result;
}

//
// Replace all occurrences of a string with another string.
//
static std::string replaceAll(const std::string &str,
                              const std::string &from,
                              const std::string &to)
{
  std::string ret = "";
  size_t pos = 0;
  while (pos < str.size()) {
    size_t found = str.find(from, pos);
    if (found == std::string::npos) {
      ret += str.substr(pos);
      break;
    }
    ret += str.substr(pos, found - pos) + to;
    pos = found + from.size();
  }
  return ret;
}

//
// Replace occurrences of special variables in the string.
//
static std::string expandVars(const std::string &str) {
#ifdef __LP64__
  return replaceAll(str, "$NATIVE_TESTS", "/data/nativetest64");
#else
  return replaceAll(str, "$NATIVE_TESTS", "/data/nativetest");
#endif
}

///
/// Helper class to kick off a run of the perfprofd daemon with a specific
/// config file.
///
class PerfProfdRunner {
 public:
  explicit PerfProfdRunner(const std::string& config_dir)
      : config_dir_(config_dir)
  {
    config_path_ = config_dir + "/" CONFIGFILE;
  }

  ~PerfProfdRunner()
  {
    remove_processed_file();
  }

  void addToConfig(const std::string &line)
  {
    config_text_ += line;
    config_text_ += "\n";
  }

  void remove_semaphore_file()
  {
    std::string semaphore(config_dir_);
    semaphore += "/" SEMAPHORE_FILENAME;
    unlink(semaphore.c_str());
  }

  void create_semaphore_file()
  {
    std::string semaphore(config_dir_);
    semaphore += "/" SEMAPHORE_FILENAME;
    close(open(semaphore.c_str(), O_WRONLY|O_CREAT, 0600));
  }

  void write_processed_file(int start_seq, int end_seq)
  {
    std::string processed = config_dir_ + "/" PROCESSED_FILENAME;
    FILE *fp = fopen(processed.c_str(), "w");
    for (int i = start_seq; i < end_seq; i++) {
      fprintf(fp, "%d\n", i);
    }
    fclose(fp);
  }

  void remove_processed_file()
  {
    std::string processed = config_dir_ + "/" PROCESSED_FILENAME;
    unlink(processed.c_str());
  }

  struct LoggingConfig : public Config {
    void Sleep(size_t seconds) override {
      // Log sleep calls but don't sleep.
      LOG(INFO) << "sleep " << seconds << " seconds";
    }

    bool IsProfilingEnabled() const override {
      //
      // Check for existence of semaphore file in config directory
      //
      if (access(config_directory.c_str(), F_OK) == -1) {
        PLOG(WARNING) << "unable to open config directory " << config_directory;
        return false;
      }

      // Check for existence of semaphore file
      std::string semaphore_filepath = config_directory
          + "/" + SEMAPHORE_FILENAME;
      if (access(semaphore_filepath.c_str(), F_OK) == -1) {
        return false;
      }

      return true;
    }
  };

  int invoke()
  {
    static const char *argv[3] = { "perfprofd", "-c", "" };
    argv[2] = config_path_.c_str();

    writeConfigFile(config_path_, config_text_);

    // execute daemon main
    LoggingConfig config;
    return perfprofd_main(3, (char **) argv, &config);
  }

 private:
  std::string config_dir_;
  std::string config_path_;
  std::string config_text_;

  void writeConfigFile(const std::string &config_path,
                       const std::string &config_text)
  {
    FILE *fp = fopen(config_path.c_str(), "w");
    ASSERT_TRUE(fp != nullptr);
    fprintf(fp, "%s\n", config_text.c_str());
    fclose(fp);
  }
};

//......................................................................

static std::string encoded_file_path(const std::string& dest_dir,
                                     int seq) {
  return android::base::StringPrintf("%s/perf.data.encoded.%d",
                                     dest_dir.c_str(), seq);
}

static void readEncodedProfile(const std::string& dest_dir,
                               const char *testpoint,
                               wireless_android_play_playlog::AndroidPerfProfile &encodedProfile)
{
  struct stat statb;
  int perf_data_stat_result = stat(encoded_file_path(dest_dir, 0).c_str(), &statb);
  ASSERT_NE(-1, perf_data_stat_result);

  // read
  std::string encoded;
  encoded.resize(statb.st_size);
  FILE *ifp = fopen(encoded_file_path(dest_dir, 0).c_str(), "r");
  ASSERT_NE(nullptr, ifp);
  size_t items_read = fread((void*) encoded.data(), statb.st_size, 1, ifp);
  ASSERT_EQ(1, items_read);
  fclose(ifp);

  // decode
  encodedProfile.ParseFromString(encoded);
}

static std::string encodedLoadModuleToString(const wireless_android_play_playlog::LoadModule &lm)
{
  std::stringstream ss;
  ss << "name: \"" << lm.name() << "\"\n";
  if (lm.build_id() != "") {
    ss << "build_id: \"" << lm.build_id() << "\"\n";
  }
  for (const auto& symbol : lm.symbol()) {
    ss << "symbol: \"" << symbol << "\"\n";
  }
  return ss.str();
}

static std::string encodedModuleSamplesToString(const wireless_android_play_playlog::LoadModuleSamples &mod)
{
  std::stringstream ss;

  ss << "load_module_id: " << mod.load_module_id() << "\n";
  for (size_t k = 0; k < mod.address_samples_size(); k++) {
    const auto &sample = mod.address_samples(k);
    ss << "  address_samples {\n";
    for (size_t l = 0; l < mod.address_samples(k).address_size();
         l++) {
      auto address = mod.address_samples(k).address(l);
      ss << "    address: " << address << "\n";
    }
    ss << "    count: " << sample.count() << "\n";
    ss << "  }\n";
  }
  return ss.str();
}

#define RAW_RESULT(x) #x

//
// Check to see if the log messages emitted by the daemon
// match the expected result. By default we use a partial
// match, e.g. if we see the expected excerpt anywhere in the
// result, it's a match (for exact match, set exact to true)
//
static void compareLogMessages(const std::string &actual,
                               const std::string &expected,
                               const char *testpoint,
                               bool exactMatch=false)
{
   std::string sqexp = squeezeWhite(expected, "expected");
   std::string sqact = squeezeWhite(actual, "actual");
   if (exactMatch) {
     EXPECT_STREQ(sqexp.c_str(), sqact.c_str());
   } else {
     std::size_t foundpos = sqact.find(sqexp);
     bool wasFound = true;
     if (foundpos == std::string::npos) {
       std::cerr << testpoint << ": expected result not found\n";
       std::cerr << " Actual: \"" << sqact << "\"\n";
       std::cerr << " Expected: \"" << sqexp << "\"\n";
       wasFound = false;
     }
     EXPECT_TRUE(wasFound);
   }
}

TEST_F(PerfProfdTest, TestUtil)
{
  EXPECT_EQ("", replaceAll("", "", ""));
  EXPECT_EQ("zzbc", replaceAll("abc", "a", "zz"));
  EXPECT_EQ("azzc", replaceAll("abc", "b", "zz"));
  EXPECT_EQ("abzz", replaceAll("abc", "c", "zz"));
  EXPECT_EQ("xxyyzz", replaceAll("abc", "abc", "xxyyzz"));
}

TEST_F(PerfProfdTest, MissingGMS)
{
  //
  // AWP requires cooperation between the daemon and the GMS core
  // piece. If we're running on a device that has an old or damaged
  // version of GMS core, then the config directory we're interested in
  // may not be there. This test insures that the daemon does the
  // right thing in this case.
  //
  PerfProfdRunner runner(conf_dir);
  runner.addToConfig("only_debug_build=0");
  runner.addToConfig("trace_config_read=0");
  runner.addToConfig("config_directory=/does/not/exist");
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("use_fixed_seed=1");
  runner.addToConfig("collection_interval=100");

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // Verify log contents
  const std::string expected = RAW_RESULT(
      I: sleep 90 seconds
      W: unable to open config directory /does/not/exist: No such file or directory
      I: profile collection skipped (missing config directory)
                                          );

  // check to make sure entire log matches
  compareLogMessages(JoinTestLog(" "), expected, "MissingGMS");
}


TEST_F(PerfProfdTest, MissingOptInSemaphoreFile)
{
  //
  // Android device owners must opt in to "collect and report usage
  // data" in order for us to be able to collect profiles. The opt-in
  // check is performed in the GMS core component; if the check
  // passes, then it creates a semaphore file for the daemon to pick
  // up on.
  //
  PerfProfdRunner runner(conf_dir);
  runner.addToConfig("only_debug_build=0");
  std::string cfparam("config_directory="); cfparam += conf_dir;
  runner.addToConfig(cfparam);
  std::string ddparam("destination_directory="); ddparam += dest_dir;
  runner.addToConfig(ddparam);
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("use_fixed_seed=1");
  runner.addToConfig("collection_interval=100");

  runner.remove_semaphore_file();

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // Verify log contents
  const std::string expected = RAW_RESULT(
      I: profile collection skipped (missing config directory)
                                          );
  // check to make sure log excerpt matches
  compareLogMessages(JoinTestLog(" "), expected, "MissingOptInSemaphoreFile");
}

TEST_F(PerfProfdTest, MissingPerfExecutable)
{
  //
  // Perfprofd uses the 'simpleperf' tool to collect profiles
  // (although this may conceivably change in the future). This test
  // checks to make sure that if 'simpleperf' is not present we bail out
  // from collecting profiles.
  //
  PerfProfdRunner runner(conf_dir);
  runner.addToConfig("only_debug_build=0");
  runner.addToConfig("trace_config_read=1");
  std::string cfparam("config_directory="); cfparam += conf_dir;
  runner.addToConfig(cfparam);
  std::string ddparam("destination_directory="); ddparam += dest_dir;
  runner.addToConfig(ddparam);
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("use_fixed_seed=1");
  runner.addToConfig("collection_interval=100");
  runner.addToConfig("perf_path=/does/not/exist");

  // Create semaphore file
  runner.create_semaphore_file();

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // expected log contents
  const std::string expected = RAW_RESULT(
      I: profile collection skipped (missing 'perf' executable)
                                          );
  // check to make sure log excerpt matches
  compareLogMessages(JoinTestLog(" "), expected, "MissingPerfExecutable");
}

TEST_F(PerfProfdTest, BadPerfRun)
{
  //
  // Perf tools tend to be tightly coupled with a specific kernel
  // version -- if things are out of sync perf could fail or
  // crash. This test makes sure that we detect such a case and log
  // the error.
  //
  PerfProfdRunner runner(conf_dir);
  runner.addToConfig("only_debug_build=0");
  std::string cfparam("config_directory="); cfparam += conf_dir;
  runner.addToConfig(cfparam);
  std::string ddparam("destination_directory="); ddparam += dest_dir;
  runner.addToConfig(ddparam);
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("use_fixed_seed=1");
  runner.addToConfig("collection_interval=100");
  runner.addToConfig("perf_path=/system/bin/false");

  // Create semaphore file
  runner.create_semaphore_file();

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // Verify log contents
  const std::string expected = RAW_RESULT(
      W: perf bad exit status 1
      W: profile collection failed
                                          );

  // check to make sure log excerpt matches
  compareLogMessages(JoinTestLog(" "), expected, "BadPerfRun");
}

TEST_F(PerfProfdTest, ConfigFileParsing)
{
  //
  // Gracefully handly malformed items in the config file
  //
  PerfProfdRunner runner(conf_dir);
  runner.addToConfig("only_debug_build=0");
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("collection_interval=100");
  runner.addToConfig("use_fixed_seed=1");
  runner.addToConfig("destination_directory=/does/not/exist");

  // assorted bad syntax
  runner.addToConfig("collection_interval=0");
  runner.addToConfig("collection_interval=-1");
  runner.addToConfig("collection_interval=2");
  runner.addToConfig("nonexistent_key=something");
  runner.addToConfig("no_equals_stmt");

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  EXPECT_EQ(0, daemon_main_return_code);

  // Verify log contents
  const std::string expected = RAW_RESULT(
      W: line 6: specified value 0 for 'collection_interval' outside permitted range [100 4294967295] (ignored)
      W: line 7: malformed unsigned value (ignored)
      W: line 8: specified value 2 for 'collection_interval' outside permitted range [100 4294967295] (ignored)
      W: line 9: unknown option 'nonexistent_key' ignored
      W: line 10: line malformed (no '=' found)
                                          );

  // check to make sure log excerpt matches
  compareLogMessages(JoinTestLog(" "), expected, "ConfigFileParsing");
}

TEST_F(PerfProfdTest, ProfileCollectionAnnotations)
{
  unsigned util1 = collect_cpu_utilization();
  EXPECT_LE(util1, 100);
  EXPECT_GE(util1, 0);

  // NB: expectation is that when we run this test, the device will be
  // completed booted, will be on charger, and will not have the camera
  // active.
  EXPECT_FALSE(get_booting());
  EXPECT_TRUE(get_charging());
  EXPECT_FALSE(get_camera_active());
}

TEST_F(PerfProfdTest, BasicRunWithCannedPerf)
{
  //
  // Verify the portion of the daemon that reads and encodes
  // perf.data files. Here we run the encoder on a canned perf.data
  // file and verify that the resulting protobuf contains what
  // we think it should contain.
  //
  std::string input_perf_data(test_dir);
  input_perf_data += "/canned.perf.data";

  // Set up config to avoid these annotations (they are tested elsewhere)
  ConfigReader config_reader;
  config_reader.overrideUnsignedEntry("collect_cpu_utilization", 0);
  config_reader.overrideUnsignedEntry("collect_charging_state", 0);
  config_reader.overrideUnsignedEntry("collect_camera_active", 0);
  PerfProfdRunner::LoggingConfig config;
  config_reader.FillConfig(&config);

  // Kick off encoder and check return code
  PROFILE_RESULT result =
      encode_to_proto(input_perf_data, encoded_file_path(dest_dir, 0).c_str(), config, 0, nullptr);
  ASSERT_EQ(OK_PROFILE_COLLECTION, result) << JoinTestLog(" ");

  // Read and decode the resulting perf.data.encoded file
  wireless_android_play_playlog::AndroidPerfProfile encodedProfile;
  readEncodedProfile(dest_dir,
                     "BasicRunWithCannedPerf",
                     encodedProfile);

  // Expect 45 programs
  EXPECT_EQ(45, encodedProfile.programs_size());

  // Check a couple of load modules
  { const auto &lm0 = encodedProfile.load_modules(0);
    std::string act_lm0 = encodedLoadModuleToString(lm0);
    std::string sqact0 = squeezeWhite(act_lm0, "actual for lm 0");
    const std::string expected_lm0 = RAW_RESULT(
        name: "/data/app/com.google.android.apps.plus-1/lib/arm/libcronet.so"
                                                );
    std::string sqexp0 = squeezeWhite(expected_lm0, "expected_lm0");
    EXPECT_STREQ(sqexp0.c_str(), sqact0.c_str());
  }
  { const auto &lm9 = encodedProfile.load_modules(9);
    std::string act_lm9 = encodedLoadModuleToString(lm9);
    std::string sqact9 = squeezeWhite(act_lm9, "actual for lm 9");
    const std::string expected_lm9 = RAW_RESULT(
        name: "/system/lib/libandroid_runtime.so" build_id: "8164ed7b3a8b8f5a220d027788922510"
                                                );
    std::string sqexp9 = squeezeWhite(expected_lm9, "expected_lm9");
    EXPECT_STREQ(sqexp9.c_str(), sqact9.c_str());
  }

  // Examine some of the samples now
  { const auto &p1 = encodedProfile.programs(0);
    const auto &lm1 = p1.modules(0);
    std::string act_lm1 = encodedModuleSamplesToString(lm1);
    std::string sqact1 = squeezeWhite(act_lm1, "actual for lm1");
    const std::string expected_lm1 = RAW_RESULT(
        load_module_id: 9 address_samples { address: 296100 count: 1 }
                                                );
    std::string sqexp1 = squeezeWhite(expected_lm1, "expected_lm1");
    EXPECT_STREQ(sqexp1.c_str(), sqact1.c_str());
  }
  { const auto &p1 = encodedProfile.programs(2);
    const auto &lm2 = p1.modules(0);
    std::string act_lm2 = encodedModuleSamplesToString(lm2);
    std::string sqact2 = squeezeWhite(act_lm2, "actual for lm2");
    const std::string expected_lm2 = RAW_RESULT(
        load_module_id: 2
        address_samples { address: 28030244 count: 1 }
        address_samples { address: 29657840 count: 1 }
                                                );
    std::string sqexp2 = squeezeWhite(expected_lm2, "expected_lm2");
    EXPECT_STREQ(sqexp2.c_str(), sqact2.c_str());
  }
}

TEST_F(PerfProfdTest, BasicRunWithCannedPerfWithSymbolizer)
{
  //
  // Verify the portion of the daemon that reads and encodes
  // perf.data files. Here we run the encoder on a canned perf.data
  // file and verify that the resulting protobuf contains what
  // we think it should contain.
  //
  std::string input_perf_data(test_dir);
  input_perf_data += "/canned.perf.data";

  // Set up config to avoid these annotations (they are tested elsewhere)
  ConfigReader config_reader;
  config_reader.overrideUnsignedEntry("collect_cpu_utilization", 0);
  config_reader.overrideUnsignedEntry("collect_charging_state", 0);
  config_reader.overrideUnsignedEntry("collect_camera_active", 0);
  PerfProfdRunner::LoggingConfig config;
  config_reader.FillConfig(&config);

  // Kick off encoder and check return code
  struct TestSymbolizer : public perfprofd::Symbolizer {
    std::string Decode(const std::string& dso, uint64_t address) override {
      return dso + "@" + std::to_string(address);
    }
  };
  TestSymbolizer test_symbolizer;
  PROFILE_RESULT result =
      encode_to_proto(input_perf_data,
                      encoded_file_path(dest_dir, 0).c_str(),
                      config,
                      0,
                      &test_symbolizer);
  ASSERT_EQ(OK_PROFILE_COLLECTION, result);

  // Read and decode the resulting perf.data.encoded file
  wireless_android_play_playlog::AndroidPerfProfile encodedProfile;
  readEncodedProfile(dest_dir,
                     "BasicRunWithCannedPerf",
                     encodedProfile);

  // Expect 45 programs
  EXPECT_EQ(45, encodedProfile.programs_size());

  // Check a couple of load modules
  { const auto &lm0 = encodedProfile.load_modules(0);
    std::string act_lm0 = encodedLoadModuleToString(lm0);
    std::string sqact0 = squeezeWhite(act_lm0, "actual for lm 0");
    const std::string expected_lm0 = RAW_RESULT(
        name: "/data/app/com.google.android.apps.plus-1/lib/arm/libcronet.so"
        symbol: "/data/app/com.google.android.apps.plus-1/lib/arm/libcronet.so@310106"
        symbol: "/data/app/com.google.android.apps.plus-1/lib/arm/libcronet.so@1949952"
                                                );
    std::string sqexp0 = squeezeWhite(expected_lm0, "expected_lm0");
    EXPECT_STREQ(sqexp0.c_str(), sqact0.c_str());
  }
  { const auto &lm9 = encodedProfile.load_modules(9);
    std::string act_lm9 = encodedLoadModuleToString(lm9);
    std::string sqact9 = squeezeWhite(act_lm9, "actual for lm 9");
    const std::string expected_lm9 = RAW_RESULT(
        name: "/system/lib/libandroid_runtime.so" build_id: "8164ed7b3a8b8f5a220d027788922510"
                                                );
    std::string sqexp9 = squeezeWhite(expected_lm9, "expected_lm9");
    EXPECT_STREQ(sqexp9.c_str(), sqact9.c_str());
  }

  // Examine some of the samples now
  { const auto &p1 = encodedProfile.programs(0);
    const auto &lm1 = p1.modules(0);
    std::string act_lm1 = encodedModuleSamplesToString(lm1);
    std::string sqact1 = squeezeWhite(act_lm1, "actual for lm1");
    const std::string expected_lm1 = RAW_RESULT(
        load_module_id: 9 address_samples { address: 296100 count: 1 }
                                                );
    std::string sqexp1 = squeezeWhite(expected_lm1, "expected_lm1");
    EXPECT_STREQ(sqexp1.c_str(), sqact1.c_str());
  }
  { const auto &p1 = encodedProfile.programs(2);
    const auto &lm2 = p1.modules(0);
    std::string act_lm2 = encodedModuleSamplesToString(lm2);
    std::string sqact2 = squeezeWhite(act_lm2, "actual for lm2");
    const std::string expected_lm2 = RAW_RESULT(
        load_module_id: 2
        address_samples { address: 18446744073709551615 count: 1 }
        address_samples { address: 18446744073709551614 count: 1 }
                                                );
    std::string sqexp2 = squeezeWhite(expected_lm2, "expected_lm2");
    EXPECT_STREQ(sqexp2.c_str(), sqact2.c_str());
  }
}

TEST_F(PerfProfdTest, CallchainRunWithCannedPerf)
{
  // This test makes sure that the perf.data converter
  // can handle call chains.
  //
  std::string input_perf_data(test_dir);
  input_perf_data += "/callchain.canned.perf.data";

  // Set up config to avoid these annotations (they are tested elsewhere)
  ConfigReader config_reader;
  config_reader.overrideUnsignedEntry("collect_cpu_utilization", 0);
  config_reader.overrideUnsignedEntry("collect_charging_state", 0);
  config_reader.overrideUnsignedEntry("collect_camera_active", 0);
  PerfProfdRunner::LoggingConfig config;
  config_reader.FillConfig(&config);

  // Kick off encoder and check return code
  PROFILE_RESULT result =
      encode_to_proto(input_perf_data, encoded_file_path(dest_dir, 0).c_str(), config, 0, nullptr);
  ASSERT_EQ(OK_PROFILE_COLLECTION, result);

  // Read and decode the resulting perf.data.encoded file
  wireless_android_play_playlog::AndroidPerfProfile encodedProfile;
  readEncodedProfile(dest_dir,
                     "BasicRunWithCannedPerf",
                     encodedProfile);


  // Expect 3 programs 8 load modules
  EXPECT_EQ(3, encodedProfile.programs_size());
  EXPECT_EQ(8, encodedProfile.load_modules_size());

  // Check a couple of load modules
  { const auto &lm0 = encodedProfile.load_modules(0);
    std::string act_lm0 = encodedLoadModuleToString(lm0);
    std::string sqact0 = squeezeWhite(act_lm0, "actual for lm 0");
    const std::string expected_lm0 = RAW_RESULT(
        name: "/system/bin/dex2oat"
        build_id: "ee12bd1a1de39422d848f249add0afc4"
                                                );
    std::string sqexp0 = squeezeWhite(expected_lm0, "expected_lm0");
    EXPECT_STREQ(sqexp0.c_str(), sqact0.c_str());
  }
  { const auto &lm1 = encodedProfile.load_modules(1);
    std::string act_lm1 = encodedLoadModuleToString(lm1);
    std::string sqact1 = squeezeWhite(act_lm1, "actual for lm 1");
    const std::string expected_lm1 = RAW_RESULT(
        name: "/system/bin/linker"
        build_id: "a36715f673a4a0aa76ef290124c516cc"
                                                );
    std::string sqexp1 = squeezeWhite(expected_lm1, "expected_lm1");
    EXPECT_STREQ(sqexp1.c_str(), sqact1.c_str());
  }

  // Examine some of the samples now
  { const auto &p0 = encodedProfile.programs(0);
    const auto &lm1 = p0.modules(0);
    std::string act_lm1 = encodedModuleSamplesToString(lm1);
    std::string sqact1 = squeezeWhite(act_lm1, "actual for lm1");
    const std::string expected_lm1 = RAW_RESULT(
        load_module_id: 0
        address_samples { address: 108552 count: 2 }
                                                );
    std::string sqexp1 = squeezeWhite(expected_lm1, "expected_lm1");
    EXPECT_STREQ(sqexp1.c_str(), sqact1.c_str());
  }
  { const auto &p4 = encodedProfile.programs(2);
    const auto &lm2 = p4.modules(1);
    std::string act_lm2 = encodedModuleSamplesToString(lm2);
    std::string sqact2 = squeezeWhite(act_lm2, "actual for lm2");
    const std::string expected_lm2 = RAW_RESULT(
        load_module_id: 2 address_samples { address: 403913 count: 1 } address_samples { address: 840761 count: 1 } address_samples { address: 846481 count: 1 } address_samples { address: 999053 count: 1 } address_samples { address: 1012959 count: 1 } address_samples { address: 1524309 count: 1 } address_samples { address: 1580779 count: 1 } address_samples { address: 4287986288 count: 1 }
                                                );
    std::string sqexp2 = squeezeWhite(expected_lm2, "expected_lm2");
    EXPECT_STREQ(sqexp2.c_str(), sqact2.c_str());
  }
}

TEST_F(PerfProfdTest, BasicRunWithLivePerf)
{
  //
  // Basic test to exercise the main loop of the daemon. It includes
  // a live 'perf' run
  //
  PerfProfdRunner runner(conf_dir);
  runner.addToConfig("only_debug_build=0");
  std::string ddparam("destination_directory="); ddparam += dest_dir;
  runner.addToConfig(ddparam);
  std::string cfparam("config_directory="); cfparam += conf_dir;
  runner.addToConfig(cfparam);
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("use_fixed_seed=12345678");
  runner.addToConfig("max_unprocessed_profiles=100");
  runner.addToConfig("collection_interval=9999");
  runner.addToConfig("sample_duration=2");

  // Create semaphore file
  runner.create_semaphore_file();

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  ASSERT_EQ(0, daemon_main_return_code);

  // Read and decode the resulting perf.data.encoded file
  wireless_android_play_playlog::AndroidPerfProfile encodedProfile;
  readEncodedProfile(dest_dir, "BasicRunWithLivePerf", encodedProfile);

  // Examine what we get back. Since it's a live profile, we can't
  // really do much in terms of verifying the contents.
  EXPECT_LT(0, encodedProfile.programs_size());

  // Verify log contents
  const std::string expected = std::string(
      "I: starting Android Wide Profiling daemon ") +
      "I: config file path set to " + conf_dir + "/perfprofd.conf " +
      RAW_RESULT(
      I: random seed set to 12345678
      I: sleep 674 seconds
      I: initiating profile collection
      I: sleep 2 seconds
      I: profile collection complete
      I: sleep 9325 seconds
      I: finishing Android Wide Profiling daemon
                                          );
  // check to make sure log excerpt matches
  compareLogMessages(JoinTestLog(" "), expandVars(expected), "BasicRunWithLivePerf", true);
}

TEST_F(PerfProfdTest, MultipleRunWithLivePerf)
{
  //
  // Basic test to exercise the main loop of the daemon. It includes
  // a live 'perf' run
  //
  PerfProfdRunner runner(conf_dir);
  runner.addToConfig("only_debug_build=0");
  std::string ddparam("destination_directory="); ddparam += dest_dir;
  runner.addToConfig(ddparam);
  std::string cfparam("config_directory="); cfparam += conf_dir;
  runner.addToConfig(cfparam);
  runner.addToConfig("main_loop_iterations=3");
  runner.addToConfig("use_fixed_seed=12345678");
  runner.addToConfig("collection_interval=9999");
  runner.addToConfig("sample_duration=2");
  runner.write_processed_file(1, 2);

  // Create semaphore file
  runner.create_semaphore_file();

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  ASSERT_EQ(0, daemon_main_return_code);

  // Read and decode the resulting perf.data.encoded file
  wireless_android_play_playlog::AndroidPerfProfile encodedProfile;
  readEncodedProfile(dest_dir, "BasicRunWithLivePerf", encodedProfile);

  // Examine what we get back. Since it's a live profile, we can't
  // really do much in terms of verifying the contents.
  EXPECT_LT(0, encodedProfile.programs_size());

  // Examine that encoded.1 file is removed while encoded.{0|2} exists.
  EXPECT_EQ(0, access(encoded_file_path(dest_dir, 0).c_str(), F_OK));
  EXPECT_NE(0, access(encoded_file_path(dest_dir, 1).c_str(), F_OK));
  EXPECT_EQ(0, access(encoded_file_path(dest_dir, 2).c_str(), F_OK));

  // Verify log contents
  const std::string expected = std::string(
      "I: starting Android Wide Profiling daemon ") +
      "I: config file path set to " + conf_dir + "/perfprofd.conf " +
      RAW_RESULT(
      I: random seed set to 12345678
      I: sleep 674 seconds
      I: initiating profile collection
      I: sleep 2 seconds
      I: profile collection complete
      I: sleep 9325 seconds
      I: sleep 4974 seconds
      I: initiating profile collection
      I: sleep 2 seconds
      I: profile collection complete
      I: sleep 5025 seconds
      I: sleep 501 seconds
      I: initiating profile collection
      I: sleep 2 seconds
      I: profile collection complete
      I: sleep 9498 seconds
      I: finishing Android Wide Profiling daemon
                                          );
  // check to make sure log excerpt matches
  compareLogMessages(JoinTestLog(" "), expandVars(expected), "BasicRunWithLivePerf", true);
}

TEST_F(PerfProfdTest, CallChainRunWithLivePerf)
{
  //
  // Collect a callchain profile, so as to exercise the code in
  // perf_data post-processing that digests callchains.
  //
  PerfProfdRunner runner(conf_dir);
  std::string ddparam("destination_directory="); ddparam += dest_dir;
  runner.addToConfig(ddparam);
  std::string cfparam("config_directory="); cfparam += conf_dir;
  runner.addToConfig(cfparam);
  runner.addToConfig("main_loop_iterations=1");
  runner.addToConfig("use_fixed_seed=12345678");
  runner.addToConfig("max_unprocessed_profiles=100");
  runner.addToConfig("collection_interval=9999");
  runner.addToConfig("stack_profile=1");
  runner.addToConfig("sample_duration=2");

  // Create semaphore file
  runner.create_semaphore_file();

  // Kick off daemon
  int daemon_main_return_code = runner.invoke();

  // Check return code from daemon
  ASSERT_EQ(0, daemon_main_return_code);

  // Read and decode the resulting perf.data.encoded file
  wireless_android_play_playlog::AndroidPerfProfile encodedProfile;
  readEncodedProfile(dest_dir, "CallChainRunWithLivePerf", encodedProfile);

  // Examine what we get back. Since it's a live profile, we can't
  // really do much in terms of verifying the contents.
  EXPECT_LT(0, encodedProfile.programs_size());

  // Verify log contents
  const std::string expected = std::string(
      "I: starting Android Wide Profiling daemon ") +
      "I: config file path set to " + conf_dir + "/perfprofd.conf " +
      RAW_RESULT(
      I: random seed set to 12345678
      I: sleep 674 seconds
      I: initiating profile collection
      I: sleep 2 seconds
      I: profile collection complete
      I: sleep 9325 seconds
      I: finishing Android Wide Profiling daemon
                                          );
  // check to make sure log excerpt matches
  compareLogMessages(JoinTestLog(" "), expandVars(expected), "CallChainRunWithLivePerf", true);
}

int main(int argc, char **argv) {
  // Always log to cerr, so that device failures are visible.
  android::base::SetLogger(android::base::StderrLogger);

  CHECK(android::base::Realpath(argv[0], &gExecutableRealpath));

  // switch to / before starting testing (perfprofd
  // should be location-independent)
  chdir("/");
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
