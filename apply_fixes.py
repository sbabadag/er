import json, sys 
 
def apply_fix(file, line, fix): 
    try: 
        with open(file, 'r', encoding='utf-8') as f: 
            lines = f.readlines() 
        if 1 <= line <= len(lines): 
            lines[line-1] = fix.strip() + '\n' 
            with open(file, 'w', encoding='utf-8') as f: 
                f.writelines(lines) 
            return True 
        return False 
    except Exception as e: 
        print(f"Error applying fix: {e}") 
        return False 
 
def main(): 
    try: 
        with open('fixes.json', 'r', encoding='utf-8') as f: 
            fixes = json.load(f) 
        success = True 
        for fix in fixes: 
            if not apply_fix(fix['file'], fix['line'], fix['code']): 
                print(f"Failed to apply fix to {fix['file']} line {fix['line']}") 
                success = False 
        return 0 if success else 1 
    except Exception as e: 
        print(f"Error in main: {e}") 
        return 1 
 
if __name__ == '__main__': 
    sys.exit(main()) 
