import json, sys 
def apply_fix(file, line, fix):
    with open(file, 'r') as f:
        lines = f.readlines()
        lines[line-1] = fix.strip() + '\n'
        with open(file, 'w') as f:
            f.writelines(lines)
        return True
    return False

def main():
    with open('fixes.json', 'r') as f:
        fixes = json.load(f)
    success = True
    for fix in fixes:
        if not apply_fix(fix['file'], fix['line'], fix['code']):
            print(f"Failed to apply fix to {fix['file']} line {fix['line']}")
            success = False
    return 0 if success else 

if __name__ == '__main__':
    sys.exit(main())
