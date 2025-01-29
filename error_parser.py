import sys, re, os 
def parse_errors():
    error_data = []
    for line in sys.stdin:
        match = re.search(r'(.*?):(\d+):(\d+):\s*error:(.*)', line)
        if match:
            file_path, line_num, col_num, message = match.groups()
            error_data.append({
                'file': file_path.strip(),
                'line': int(line_num),
                'col': int(col_num),
                'message': message.strip()
            })
    return error_data
if __name__ == "__main__":
    errors = parse_errors()
    for error in errors:
        print(f"ERROR_ENTRY::{error['file']}::{error['line']}::{error['col']}::{error['message']}")
