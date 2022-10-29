import re

file1 = open("log.txt", 'r')
count = 0

while True:
    count += 1
    line = file1.readline()

    re_rmdir = re.search(r".*rmdir\(\"(.*)\"\)", line)
    if re_rmdir:
        print(f"Removed directory: {re_rmdir.group(1)}")

    re_mkdir = re.search(r".*mkdir.\"(.*)\".*", line)
    if re_mkdir:
        print(f"Made directory: {re_mkdir.group(1)}")

    re_chdir = re.search(r".*chdir\(\"(.*)\"\)", line)
    if re_chdir:
        print(f"Travelled to directory: {re_chdir.group(1)}")

    re_rm = re.search(r".*unlinkat.*\"(.*)\"", line)
    if re_rm:
        print(f"Deleted file: {re_rm.group(1)}")

    re_ln = re.search(r".*.*linkat\(.*\"(.*)\".*\"(.*)\"", line)
    if re_ln:
        print(f"Created hardlink: {re_ln.group(2)} for {re_ln.group(1)}")

    re_create = re.search(r".*openat.*\"(.*)\", (.*),", line)
    if re_create:
        if re_create.group(2) == "O_WRONLY|O_CREAT|O_NOCTTY|O_NONBLOCK":
            print(f"Created file: {re_create.group(1)}")

    re_write = re.search(r".*openat.*\"(.*)\", (.*),", line)
    if re_write:
        if re_write.group(2) == "O_WRONLY|O_CREAT|O_TRUNC":
            print(f"Written into file: {re_write.group(1)}")

    re_read = re.search(r".*openat.*\"(.*)\", (.*)\).*", line)
    if re_read:
        if re_read.group(2) == "O_RDONLY":
            print(f"Reading file: {re_read.group(1)}")

    if not line:
        break

file1.close()
