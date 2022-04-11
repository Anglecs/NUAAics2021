#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint64_t);
uint32_t expr(char *, bool *);
/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_p(char *args)
{
    if (args != NULL)
    {
        bool success;
        uint32_t ans = expr(args, &success);
        if (success)
            printf("\033[1m\033[33mResult is\033[0m %#xH \033[1m\033[33min hex, is\033[0m %d \033[1m\033[33min dec\033[0m\n", ans, ans);
        else
        {
            printf("\033[1m\033[31mPlease check you expression\033[0m\n");
            return 0;
        }
    }
    else
        printf("\n");
    return 0;
}

static int cmd_x(char *args){  
    char *N = strtok(NULL," ");  
    char *EXPR = strtok(NULL," ");  
    int len;  
    vaddr_t address;  
      
    sscanf(N, "%d", &len);  
    sscanf(EXPR, "%x", &address);  
      
    printf("0x%x:",address);  
    int i;
    for(i = 0; i < len; i ++){  
        printf("%08x ",vaddr_read(address+i*4,4));  
        uint32_t x = vaddr_read(address + i * 4, 4);
        for (int j = 0; j < 4; j++) {
            printf("%02x ", x & 0xff);
            x = x >> 8;}
    }    
    printf("\n");  
    return 0;  
}

static int cmd_info(char *args)  {  
    char *arg=strtok(NULL," ");  
    if(strcmp(arg,"r") == 0){  
        for(int i=0;i<8;i++)  
            printf("%s \t%x \t%d\n",regsl[i],cpu.gpr[i]._32,cpu.gpr[i]._32);  
        printf("$eip \t%x \t%d\n", cpu.eip, cpu.eip); 
    }  
    return 0;  
} 

static int cmd_si(char *args) {
	int step;
	if (args == NULL) step = 1;
	else sscanf(args, "%d", &step);
	cpu_exec(step);
	return 0;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);

static struct {
  char *name;
  char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  {"si", "Si 10 allows the program to suspend execution after executing 10 instructions in a single step. When n is not given, it defaults to 1",cmd_si},
  {"info","Info R is used to print register status",cmd_info},
  {"x","X 10 $ESP finds the value of expression expr, takes the result as the starting memory address, and outputs n consecutive 4 bytes in hexadecimal form",cmd_x},
  {"p", "Compute the value of an expression", cmd_p}
  /* TODO: Add more commands */
};


#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void ui_mainloop(int is_batch_mode) {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  while (1) {
    char *str = rl_gets();
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef HAS_IOE
    extern void sdl_clear_event_queue(void);
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}
