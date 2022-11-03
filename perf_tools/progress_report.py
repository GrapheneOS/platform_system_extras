import sys
import os
from datetime import datetime

#find boot_progress_start line and boot_progress_enable_screen find the time difference
# return the start time string
def find_boot_progress_start_end(fp):
    start = ""
    end = ""
    for line in fp:
        if "boot_progress_start" in line:
            start = line
        if "boot_progress_enable_screen" in line and len(start):
            end = line
            break
    return [start, end]

def replace_timestamp_abs(line, timestamp_str, date_time_obj0):
    if line[:5] != timestamp_str[:5]:
        return line

    index = line.find(" ", 6)
    if index <= 0:
        return line
    substr0 = line[:index]
    substr1 = line[index:]

    try:
        date_time_obj = datetime.strptime(substr0, '%m-%d %H:%M:%S.%f')
    except ValueError:
        return line

    date_time_delta = date_time_obj - date_time_obj0
    date_time_delta_str = str(date_time_delta)
    return date_time_delta_str + substr1

def in_time_range(start, end, line):
    try:
        current_time = datetime.strptime(line[:18], '%m-%d %H:%M:%S.%f')
    except ValueError:
        return False

    if current_time >= start and current_time <= end:
        return True

    return False

def write_to_new_file(fp, output_fp, summary_fp, timestamps):
    start_timestamp_obj = datetime.strptime(timestamps[0][:18], '%m-%d %H:%M:%S.%f')
    end_timestamp_obj = datetime.strptime(timestamps[1][:18], '%m-%d %H:%M:%S.%f')

    for line in fp:
        newline = replace_timestamp_abs(line, timestamps[0][:18], start_timestamp_obj)
        output_fp.write(newline)
        if "boot_progress_" in newline and in_time_range(start_timestamp_obj, end_timestamp_obj, line):
            summary_fp.write(newline)


def main():
    filepath = sys.argv[1]
    if not os.path.isfile(filepath):
        print("File path {} does not exist. Exiting...".format(filepath))
        sys.exit()

    output_fp = open(sys.argv[2], 'w')
    summary_fp = open(sys.argv[3], 'w')

    with open(filepath, 'r', errors = 'ignore') as fp:
        timestamps = find_boot_progress_start_end(fp)
        fp.seek(0)
        write_to_new_file(fp, output_fp, summary_fp, timestamps)

    fp.close()
    output_fp.close()
    summary_fp.close()

if __name__ == '__main__':
    main()
