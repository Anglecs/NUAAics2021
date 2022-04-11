#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

enum {
  NOTYPE = 256,
  EQ , 
  NEQ , 
  AND , 
  OR , 
  MINUS , 
  POINTOR , 
  NUMBER , 
  HEX , 
  REGISTER , 
  MARK, 
  GE, 
  LE
  /* TODO: Add more token types */

};

static struct rule {
  char *regex;
  int token_type;
  int priority;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */
  
  {"\\b[0-9]+\\b",NUMBER,0},
  {"\\b0[xX][0-9a-fA-F]+\\b", HEX,0},
  {"\\$(eax|EAX|ebx|EBX|ecx|ECX|edx|EDX|ebp|EBP|esp|ESP|esi|ESI|edi|EDI|eip|EIP)",REGISTER,0},		// register
  {"\\$(([ABCD][HLX])|([abcd][hlx]))",REGISTER,0},		// register
  {"\\b[a-zA-Z_0-9]+" , MARK, 0},		// mark
  {"!=",NEQ,3},						// not equal	
  {"!",'!',6},						// not
  {"\\*",'*',5},						// mul
  {"/",'/',5},						// div
  {"\\t+",NOTYPE,0},					// tabs
  {" +",NOTYPE,0},					// spaces
  {"\\+",'+',4},						// plus
  {"-",'-',4},						// sub
  {"==", EQ,3},						// equal
  {"&&",AND,2},						// and
  {">", '>', 3},      				// greater
  {"<", '<', 3}, 						// lower
  {">=", GE, 3},						// greater or equal
  {"<=", LE, 3},						// lower or equal
  {"\\|\\|",OR,1},					// or
  {"\\(",'(',7},                      // left bracket   
  {"\\)",')',7},                      // right bracket 
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
  int priority;
} Token;

#define MAX_TOKEN_NUM 1000
Token tokens[32];
int nr_token;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
		char *tmp = e + position + 1;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);
        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch(rules[i].token_type) {
					case NOTYPE: break;
					case REGISTER:
						tokens[nr_token].type = rules[i].token_type;
						tokens[nr_token].priority = rules[i].priority; 
						strncpy (tokens[nr_token].str,tmp,substr_len-1);
						tokens[nr_token].str[substr_len-1]='\0';
						nr_token ++;
						break; 
					default:
						tokens[nr_token].type = rules[i].token_type;
						tokens[nr_token].priority = rules[i].priority;
						strncpy (tokens[nr_token].str,substr_start,substr_len);
						tokens[nr_token].str[substr_len]='\0';
						nr_token ++;
						break;
				}
				break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}
bool check_parentheses(int start, int end)
{
    int i;
	if (tokens[start].type == '(' && tokens[end].type ==')')
	{
		int lc = 0, rc = 0;
		for (i = start + 1; i < end; i ++)
		{
			if (tokens[i].type == '(')lc ++;
			if (tokens[i].type == ')')rc ++;
			if (rc > lc) return false;	//右括号数目不能超过左边括号数目
		}
		if (lc == rc) return true;
	}
	return false;
}

int find_dominanted_op (int p,int q)
{
	int i;
	int min_priority = 10;
	int oper = p;
	int cnt = 0;
	for (i = p; i <= q;i ++)
	{
		if (tokens[i].type == NUMBER || tokens[i].type == HEX || tokens[i].type == REGISTER || tokens[i].type == MARK)
			continue;
		
		if (tokens[i].type == '(') cnt++;
		if (tokens[i].type == ')') cnt--;
		if (cnt != 0) continue;
		
		if (tokens[i].priority <= min_priority) {
			min_priority=tokens[i].priority; oper=i;
		}
 	}
	return oper;
}


int eval(int start, int end)
{
    if (start > end)
        Assert(0, "Please check you expression\n");
    else if (start == end)
		{
		uint32_t num = 0;
		if (tokens[start].type == NUMBER)
			sscanf(tokens[start].str,"%d",&num);
		else if (tokens[start].type == HEX)
			sscanf(tokens[start].str,"%x",&num);
		else if (tokens[start].type == REGISTER)
		{
			if (strlen (tokens[start].str) == 3) {
				int i;
				for (i = R_EAX; i <= R_EDI; i ++)
					if (strcmp (tokens[start].str,regsl[start]) == 0) break;
				if (i > R_EDI)
					if (strcmp (tokens[start].str,"eip") == 0)
						num = cpu.eip;
					else Assert (1,"no this register!\n");
				else num = reg_l(i);
			}
			else if (strlen (tokens[start].str) == 2) {
				if (tokens[start].str[1] == 'x' || tokens[start].str[1] == 'p' || tokens[start].str[1] == 'i') {
					int i;
					for (i = R_AX; i <= R_DI; i ++)
						if (strcmp (tokens[start].str,regsw[i]) == 0)break;
					num = reg_w(i);
				}
				else if (tokens[start].str[1] == 'l' || tokens[start].str[1] == 'h') {
					int i;
					for (i = R_AL; i <= R_BH; i ++)
						if (strcmp (tokens[start].str,regsb[i]) == 0)break;
					num = reg_b(i);
				}
				else assert (1);
			}
		}
		else
		{
			return -1;
		}
		return num;
	}
    else if (check_parentheses(start, end) == true)
        return eval(start + 1, end - 1);
    else
    {
		int op = find_dominanted_op (start,end);
 		if (start == op || tokens[op].type == POINTOR || tokens[op].type == MINUS || tokens[op].type == '!')
		{
			uint32_t val = eval (start + 1, end);
			switch (tokens[start].type)
 			{
				case POINTOR:  return vaddr_read(val, 4);
				case MINUS:return -val;
				case '!':return !val;
				default:
					return -1;
			}
		}
		uint32_t val1 = eval (start, op - 1);
		uint32_t val2 = eval (op + 1, end);
		switch (tokens[op].type)
		{
			case '+':return val1 + val2;
			case '-':return val1 - val2;
			case '*':return val1 * val2;
			case '/':return val1 / val2;
			case EQ:return val1 == val2;
			case NEQ:return val1 != val2;
			case '>':return val1 > val2;
			case '<':return val1 < val2;
			case GE:return val1 >= val2;
			case LE:return val1 <= val2;
			case AND:return val1 && val2;
			case OR:return val1 || val2;
			default:

				return -1;
  		}
    }
}



uint32_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
	int i;
	for (i = 0;i < nr_token; i ++) { 

		if (tokens[i].type == '*' && (i == 0 || (tokens[i - 1].type != NUMBER && tokens[i - 1].type != HEX && tokens[i - 1].type != REGISTER && tokens[i - 1].type != MARK && tokens[i - 1].type !=')'))) {
			tokens[i].type = POINTOR;
			tokens[i].priority = 6;
		}
		if (tokens[i].type == '-' && (i == 0 || (tokens[i - 1].type != NUMBER && tokens[i - 1].type != HEX && tokens[i - 1].type != REGISTER && tokens[i - 1].type != MARK && tokens[i - 1].type !=')'))) {
			tokens[i].type = MINUS;
			tokens[i].priority = 6;
 		}
  	}
  return eval(0, nr_token - 1);
}
