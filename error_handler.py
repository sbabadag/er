import sys, json, re 
 
def parse_compiler_error(line): 
    pattern = re.compile(r'(.*?):(\d+):(\d+):\s*(error|warning|note):\s*(.*)') 
    match = pattern.search(line) 
    if match and match.group(4).lower() == 'error': 
        return { 
            'file': match.group(1).strip(), 
            'line': int(match.group(2)), 
            'message': match.group(5).strip() 
        } 
    return None 
 
if __name__ == '__main__': 
    errors = [] 
    for line in sys.stdin: 
        error = parse_compiler_error(line) 
        if error: 
            errors.append(error) 
    with open('build_errors.json', 'w') as f: 
        json.dump(errors, f, indent=2) 
    for error in errors: 
        print(f"{error['file']}||{error['line']}||{error['message']}") 
