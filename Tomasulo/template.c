#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXLINELENGTH 1000 /* 机器指令的最大长度 */
#define MEMSIZE 10000      /* 内存的最大容量     */
#define NUMREGS 32         /* 寄存器数量         */

/*
 * 操作码和功能码定义
 */

#define regRegALU 0 /* 寄存器-寄存器的ALU运算的操作码为0 */
#define LW 35
#define SW 43
#define ADDI 8
#define ANDI 12
#define BEQZ 4
#define J 2
#define HALT 1
#define NOOP 3
#define addFunc 32 /* ALU运算的功能码 */
#define subFunc 34
#define andFunc 36

#define NOOPINSTRUCTION 0x0c000000;

/*
 * 执行单元
 */
#define LOAD1 1
#define LOAD2 2
#define STORE1 3
#define STORE2 4
#define INT1 5
#define INT2 6

#define NUMUNITS 6                                                                 /* 执行单元数量 */
char *unitname[NUMUNITS] = {"LOAD1", "LOAD2", "STORE1", "STORE2", "INT1", "INT2"}; /* 执行单元的名称 */

/*
 * 不同操作所需要的周期数
 */
#define BRANCHEXEC 3 /* 分支操作 */
#define LDEXEC 2     /* Load     */
#define STEXEC 2     /* Store    */
#define INTEXEC 1    /* 整数运算 */

/*
 * 指令状态
 */
#define ISSUING 0                                                              /* 发射   */
#define EXECUTING 1                                                            /* 执行   */
#define WRITINGRESULT 2                                                        /* 写结果 */
#define COMMITTING 3                                                           /* 提交   */
char *statename[4] = {"ISSUING", "EXECUTING", "WRITINGRESULT", "COMMTITTING"}; /*  状态名称 */

#define RBSIZE 16 /* ROB有16个单元 */
#define BTBSIZE 8 /* 分支预测缓冲栈有8个单元 */

/*
 * 2 bit 分支预测状态
 */
#define STRONGNOT 0
#define WEAKTAKEN 1
#define WEAKNOT 2
#define STRONGTAKEN 3

/*
 * 分支跳转结果
 */
#define NOTTAKEN 0
#define TAKEN 1

typedef struct _resStation
{            /* 保留栈的数据结构 */
  int instr; /*    指令    */
  int busy;  /* 空闲标志位 */
  int Vj;    /* Vj, Vk 存放操作数 */
  int Vk;
  int Qj;         /* Qj, Qk 存放将会生成结果的执行单元编号 */
  int Qk;         /* 为零则表示对应的V有效 */
  int exTimeLeft; /* 指令执行的剩余时间 */
  int reorderNum; /* 该指令对应的ROB项编号 */
} resStation;

typedef struct _reorderEntry
{                   /* ROB项的数据结构 */
  int busy;         /* 空闲标志位 */
  int instr;        /* 指令 */
  int execUnit;     /* 执行单元编号 */
  int instrStatus;  /* 指令的当前状态 */
  int valid;        /* 表明结果是否有效的标志位 */
  int result;       /* 在提交之前临时存放结果 */
  int storeAddress; /* store指令的内存地址 */
  int realBeqAddress;
  int pc;
  int addValid;
} reorderEntry;

typedef struct _regResultEntry
{                 /* 寄存器状态的数据结构 */
  int valid;      /* 1表示寄存器值有效, 否则0 */
  int reorderNum; /* 如果值无效, 记录ROB中哪个项目会提交结果 */
} regResultEntry;

typedef struct _btbEntry
{                   /* 分支预测缓冲栈的数据结构 */
  int valid;        /* 有效位 */
  int branchPC;     /* 分支指令的PC值 */
  int branchTarget; /* when predict taken, update PC with target */
  int branchPred;   /* 预测：2-bit分支历史 */
} btbEntry;

typedef struct _machineState
{                                    /* 虚拟机状态的数据结构 */
  int pc;                            /* PC */
  int cycles;                        /* 已经过的周期数 */
  resStation reservation[NUMUNITS];  /* 保留栈 */
  reorderEntry reorderBuf[RBSIZE];   /* ROB */
  regResultEntry regResult[NUMREGS]; /* 寄存器状态 */
  btbEntry btBuf[BTBSIZE];           /* 分支预测缓冲栈 */
  int btBufidx;
  int memory[MEMSIZE];               /* 内存   */
  int regFile[NUMREGS];              /* 寄存器 */
} machineState;

int field0(int);
int field1(int);
int field2(int);
int opcode(int);

void printInstruction(int);

void printState(machineState *statePtr, int memorySize)
{
  int i;

  printf("Cycles: %d\n", statePtr->cycles);

  printf("\t pc=%d\n", statePtr->pc);

  printf("\t Reservation stations:\n");
  for (i = 0; i < NUMUNITS; i++)
  {
    if (statePtr->reservation[i].busy == 1)
    {
      printf("\t \t Reservation station %d: ", i);
      if (statePtr->reservation[i].Qj == 0)
      {
        printf("Vj = %d ", statePtr->reservation[i].Vj);
      }
      else
      {
        printf("Qj = '%s' ", unitname[statePtr->reservation[i].Qj - 1]);
      }
      if (statePtr->reservation[i].Qk == 0)
      {
        printf("Vk = %d ", statePtr->reservation[i].Vk);
      }
      else
      {
        printf("Qk = '%s' ", unitname[statePtr->reservation[i].Qk - 1]);
      }
      printf(" ExTimeLeft = %d  RBNum = %d\n",
             statePtr->reservation[i].exTimeLeft,
             statePtr->reservation[i].reorderNum);
    }
  }

  printf("\t Reorder buffers:\n");
  for (i = 0; i < RBSIZE; i++)
  {
    if (statePtr->reorderBuf[i].busy == 1)
    {
      printf("\t \t Reorder buffer %d: ", i);
      printf("instr %d  executionUnit '%s'  state %s  valid %d  result %d storeAddress %d\n",
             statePtr->reorderBuf[i].instr,
             //  unitname[statePtr->reorderBuf[i].instr, statePtr->reorderBuf[i].execUnit - 1],
             unitname[statePtr->reorderBuf[i].execUnit],
             statename[statePtr->reorderBuf[i].instrStatus],
             statePtr->reorderBuf[i].valid, statePtr->reorderBuf[i].result,
             statePtr->reorderBuf[i].storeAddress);
    }
  }

  printf("\t Register result status:\n");
  for (i = 1; i < NUMREGS; i++)
  {
    if (!statePtr->regResult[i].valid)
    {
      printf("\t \t Register %d: ", i);
      printf("waiting for reorder buffer number %d\n",
             statePtr->regResult[i].reorderNum);
    }
  }

  /*
   * [TOD]如果你实现了动态分支预测, 将这里的注释取消
   */

  printf("\t Branch target buffer:\n");
  for (i=0; i<BTBSIZE; i++){
    if (statePtr->btBuf[i].valid){
      printf("\t \t Entry %d: PC=%d, Target=%d, Pred=%d\n",
       i, statePtr->btBuf[i].branchPC, statePtr->btBuf[i].branchTarget,
       statePtr->btBuf[i].branchPred);
   }
  }

  printf("\t Memory:\n");
  for (i = 0; i < memorySize; i++)
  {
    printf("\t \t memory[%d] = %d\n", i, statePtr->memory[i]);
  }

  printf("\t Registers:\n");
  for (i = 0; i < NUMREGS; i++)
  {
    printf("\t \t regFile[%d] = %d\n", i, statePtr->regFile[i]);
  }
}

//立即数指令使用
int convertNum16(int num)
{
  /* convert an 16 bit number into a 32-bit or 64-bit number */
  if (num & 0x8000)
  {
    num -= 65536;
  }
  return (num);
}

//跳转指令使用
int convertNum26(int num)
{
  /* convert an 26 bit number into a 32-bit or 64-bit number */
  if (num & 0x200000)
  {
    num -= 67108864;
  }
  return (num);
}

/*
 *[]
 *这里对指令进行解码，转换成程序可以识别的格式，需要根据指令格式来进行。
 *可以考虑使用高级语言中的位和逻辑运算
 */
int field0(int instruction)
{
  /*
   *[]
   *返回指令的第一个寄存器RS1
   */
  return (instruction >> 21) & 0x1f;
}

int field1(int instruction)
{
  /*
   *[]
   *返回指令的第二个寄存器，RS2或者Rd
   */
  return (instruction >> 16) & 0x1f;
}

int field2(int instruction)
{
  /*
   *[]
   *返回指令的第三个寄存器，Rd
   */
  return (instruction >> 11) & 0x1f;
}

int immediate(int instruction)
{
  /*
   *[]
   *返回I型指令的立即数部分
   */
  return convertNum16(instruction & 0xffff);
}

int jumpAddr(int instruction)
{
  /*
   *[]
   *返回J型指令的跳转地址
   */
  return convertNum26(instruction & 0x3ffffff);
}

int opcode(int instruction)
{
  /*
   *[]
   *返回指令的操作码
   */
  return (instruction >> 26) & 0x3f;
}

int func(int instruction)
{
  /*
   *[]
   *返回R型指令的功能域
   */
  return instruction & 0x7ff;
}

void printInstruction(int instr)
{
  char opcodeString[10];
  char funcString[11];
  int funcCode;
  int op;

  if (opcode(instr) == regRegALU)
  {
    funcCode = func(instr);
    if (funcCode == addFunc)
    {
      strcpy(opcodeString, "add");
    }
    else if (funcCode == subFunc)
    {
      strcpy(opcodeString, "sub");
    }
    else if (funcCode == andFunc)
    {
      strcpy(opcodeString, "and");
    }
    else
    {
      strcpy(opcodeString, "alu");
    }
    printf("%s %d %d %d \n", opcodeString, field0(instr), field1(instr),
           field2(instr));
  }
  else if (opcode(instr) == LW)
  {
    strcpy(opcodeString, "lw");
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           immediate(instr));
  }
  else if (opcode(instr) == SW)
  {
    strcpy(opcodeString, "sw");
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           immediate(instr));
  }
  else if (opcode(instr) == ADDI)
  {
    strcpy(opcodeString, "addi");
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           immediate(instr));
  }
  else if (opcode(instr) == ANDI)
  {
    strcpy(opcodeString, "andi");
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           immediate(instr));
  }
  else if (opcode(instr) == BEQZ)
  {
    strcpy(opcodeString, "beqz");
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           immediate(instr));
  }
  else if (opcode(instr) == J)
  {
    strcpy(opcodeString, "j");
    printf("%s %d\n", opcodeString, jumpAddr(instr));
  }
  else if (opcode(instr) == HALT)
  {
    strcpy(opcodeString, "halt");
    printf("%s\n", opcodeString);
  }
  else if (opcode(instr) == NOOP)
  {
    strcpy(opcodeString, "noop");
    printf("%s\n", opcodeString);
  }
  else
  {
    strcpy(opcodeString, "data");
    printf("%s %d\n", opcodeString, instr);
  }
}

void updateRes(int unit, machineState *statePtr, int value)
{
  /*
   *[]
   * 更新保留栈:
   * 将位于公共数据总线上的数据
   * 复制到正在等待它的其他保留栈中去
   */
  for (int i = 0; i < NUMUNITS; i++)
  {
    resStation *sta = &statePtr->reservation[i];
    if (sta->busy)
    {
      if (sta->Qj - 1 == unit)
      {
        sta->Vj = value;
        sta->Qj = 0;
      }
      if (sta->Qk - 1 == unit)
      {
        sta->Vk = value;
        sta->Qk = 0;
      }
    }
  }
}

void fixVj(resStation *sta, machineState *statePtr, int Rj)
{
  if (statePtr->regResult[Rj].valid)
  {
    sta->Vj = statePtr->regFile[Rj];
    sta->Qj = 0;
  }
  else
  {
    int rob_id = statePtr->regResult[Rj].reorderNum;
    if (statePtr->reorderBuf[rob_id].valid)
    { //????是这个判断条件吗
      sta->Vj = statePtr->reorderBuf[rob_id].result;
      sta->Qj = 0;
    }
    else
    {
      sta->Qj = statePtr->reorderBuf[rob_id].execUnit + 1;
    }
  }
}

void fixVk(resStation *sta, machineState *statePtr, int Rk)
{
  if (statePtr->regResult[Rk].valid)
  {
    sta->Vk = statePtr->regFile[Rk];
    sta->Qk = 0;
  }
  else
  {
    int rob_id = statePtr->regResult[Rk].reorderNum;
    if (statePtr->reorderBuf[rob_id].valid)
    { //????是这个判断条件吗
      sta->Vk = statePtr->reorderBuf[rob_id].result;
      sta->Qk = 0;
    }
    else
    {
      sta->Qk = statePtr->reorderBuf[rob_id].execUnit + 1;
    }
  }
}

void issueInstr(int instr, int unit, machineState *statePtr, int reorderNum)
{
  /*
   * []
   * 发射指令:
   * 填写保留栈和ROB项的内容.
   * 注意, 要在所有的字段中写入正确的值.
   * 检查寄存器状态, 相应的在Vj,Vk和Qj,Qk字段中设置正确的值:
   * 对于I类型指令, 设置Qk=0,Vk=0; ///????Vk
   * 对于sw指令, 如果寄存器有效, 将寄存器中的内存基地址保存在Vj中; ///???这么写，没理解
   * 对于beqz和j指令, 将当前PC+1的值保存在Vk字段中.
   * 如果指令在提交时会修改寄存器的值, 还需要在这里更新寄存器状态数据结构.
   */
  reorderEntry *rob = &statePtr->reorderBuf[reorderNum];
  rob->busy = 1;
  rob->instr = instr;
  rob->execUnit = unit;
  rob->instrStatus = ISSUING;
  rob->valid = 0;
  rob->result = 0;
  rob->pc = statePtr->pc;

  resStation *sta = &statePtr->reservation[unit];
  sta->instr = instr;
  sta->busy = 1;
  int Rj, Rk, Rd;
  if (opcode(instr) == regRegALU)
  {
    sta->exTimeLeft = INTEXEC;
    sta->reorderNum = reorderNum;
    Rj = field0(instr);
    Rk = field1(instr);
    fixVj(sta, statePtr, Rj);
    fixVk(sta, statePtr, Rk);

    Rd = field2(instr);
    statePtr->regResult[Rd].valid = 0;
    statePtr->regResult[Rd].reorderNum = reorderNum;
  }
  else if (opcode(instr) == ADDI || opcode(instr) == ANDI)
  {
    sta->exTimeLeft = INTEXEC;
    sta->reorderNum = reorderNum;
    Rj = field0(instr);
    fixVj(sta, statePtr, Rj);
    sta->Vk = 0;
    sta->Qk = 0;

    Rd = field1(instr);
    statePtr->regResult[Rd].valid = 0;
    statePtr->regResult[Rd].reorderNum = reorderNum;
  }
  else if (opcode(instr) == LW)
  {
    sta->exTimeLeft = LDEXEC;
    sta->reorderNum = reorderNum;
    Rj = field0(instr);
    fixVj(sta, statePtr, Rj);
    sta->Vk = 0;
    sta->Qk = 0;

    Rd = field1(instr);
    statePtr->regResult[Rd].valid = 0;
    statePtr->regResult[Rd].reorderNum = reorderNum;
  }
  else if (opcode(instr) == SW)
  {
    sta->exTimeLeft = STEXEC;
    sta->reorderNum = reorderNum;
    Rj = field0(instr);
    fixVj(sta, statePtr, Rj);

    Rk = field1(instr);
    fixVk(sta, statePtr, Rk);
    // rob->storeAddress = immediate(instr); //??我的理解正确吗？
  }
  else if (opcode(instr) == BEQZ)
  {
    sta->exTimeLeft = BRANCHEXEC;
    sta->reorderNum = reorderNum;
    Rj = field0(instr);
    fixVj(sta, statePtr, Rj);
    sta->Vk = statePtr->pc + 1;
    sta->Qk = 0;
  }
  else if (opcode(instr) == J)
  {
    sta->exTimeLeft = INTEXEC;
    sta->reorderNum = reorderNum;
    sta->Vj = 0;
    sta->Qj = 0;
    sta->Vk = statePtr->pc + 1;
    sta->Qk = 0;
  }
  else if (opcode(instr) == HALT)
  {
    sta->exTimeLeft = INTEXEC;
    sta->reorderNum = reorderNum;
    sta->Qj = 0;
    sta->Qk = 0;
  }
  else if (opcode(instr) == NOOP)
  {
    sta->exTimeLeft = INTEXEC;
    sta->reorderNum = reorderNum;
    sta->Qj = 0;
    sta->Qk = 0;
  }
  else
  {
    printf("invalid instruction %d\n", instr);
    exit(0);
  }
}

int checkReorder(machineState *statePtr, int *headRB, int *tailRB)
{ //?
  /*
   * []
   * 在ROB的队尾检查是否有空闲的空间, 如果有, 返回空闲项目的编号.
   * ROB是一个循环队列, 它可以容纳RBSIZE个项目.
   * 新的指令被添加到队列的末尾, 指令提交则是从队首进行的.
   * 当队列的首指针或尾指针到达数组中的最后一项时, 它应滚动到数组的第一项.
   */
  if ((*tailRB + 1) % RBSIZE != *headRB || !statePtr->reorderBuf[(*tailRB + 1) % RBSIZE].busy)
  {
    *tailRB = (*tailRB + 1) % RBSIZE;
    return (*tailRB);
  }
  else
  {
    return -1;
  }
}

int getResult(resStation rStation, machineState *statePtr)
{
  int op, immed, function, address;

  /*
   * [TODO]
   * 这个函数负责计算有输出的指令的结果.
   * 你需要完成下面的case语句....
   */

  op = opcode(rStation.instr);
  immed = immediate(rStation.instr);
  address = jumpAddr(rStation.instr);

  switch (op)
  {
  case ANDI:
    return (rStation.Vj & immed);
    break;
  case ADDI:
    return (rStation.Vj + immed);
    break;
  case regRegALU:
    function = func(rStation.instr);
    switch (function)
    {
    case addFunc:
      return (rStation.Vj + rStation.Vk);
      break;
    case subFunc:
      return (rStation.Vj - rStation.Vk);
      break;
    case andFunc:
      return (rStation.Vj & rStation.Vk);
    default:
      return 0;
      break;
    }
    break;
  case LW:
    return statePtr->memory[rStation.Vj + immed]; // RAW冲突怎么解决？
    break;
  case SW:
    statePtr->reorderBuf[rStation.reorderNum].storeAddress = rStation.Vj + immed;
    return rStation.Vk;
    break;
  case BEQZ:
    if (rStation.Vj != 0)
      statePtr->reorderBuf[rStation.reorderNum].addValid = 0;
    else
      statePtr->reorderBuf[rStation.reorderNum].addValid = 1;
    return (rStation.Vk + immed);
    break;
  case J:
    return (rStation.Vk + address);
    break;
  default:
    return 0;
    break;
  }
}

/* 选作内容 */
int getPrediction(machineState *statePtr)
{
  /*
   * [TOD]
   * 对给定的PC, 检查分支预测缓冲栈中是否有历史记录
   * 如果有, 返回根据历史信息进行的预测, 否则返回-1
   */
  for(int i = 0; i < BTBSIZE; i++){
    if(statePtr->btBuf[i].valid == 1 && statePtr->btBuf[i].branchPC == statePtr->pc && (statePtr->btBuf[i].branchPred == WEAKTAKEN || statePtr->btBuf[i].branchPred == STRONGTAKEN)){
      return statePtr->btBuf[i].branchTarget;
    }
  }

  return -1;
}

int changeBtb(int from, int result){
  switch (from)
  {
  case WEAKTAKEN:
    if(result == 0){
      from = WEAKNOT;
    }else{
      from = STRONGTAKEN;
    }
    break;
  case STRONGTAKEN:
    if(result == 0){
      from = WEAKTAKEN;
    }
  break;
  case WEAKNOT:
    if(result == 0){
      from = STRONGNOT;
    }else{
      from = WEAKTAKEN;
    }
  break;
  case STRONGNOT:
    if(result == 1){
      from = WEAKNOT;
    }
  break;
  default:
    break;
  }
  return from;
}

/* 选作内容 */
void updateBTB(machineState *statePtr, int branchPC, int targetPC, int outcome)
{
  /*
   * [TOD]
   * 更新分支预测缓冲栈: 检查是否与缓冲栈中的项目匹配.
   * 如果是, 对2-bit的历史记录进行更新;
   * 如果不是, 将当前的分支语句添加到缓冲栈中去.
   * 如果缓冲栈已满，你需要选择一种替换算法将旧的记录替换出去.
   * 如果当前跳转成功, 将初始的历史状态设置为STRONGTAKEN;
   * 如果不成功, 将历史设置为STRONGNOT
   */
  for(int i = 0; i < BTBSIZE; i++){
    if(statePtr->btBuf[i].valid == 1 && statePtr->btBuf[i].branchPC == branchPC){
      btbEntry* btb = &statePtr->btBuf[i];
      btb->branchPred = changeBtb(btb->branchPred, outcome);
      return;
    }
  }

  btbEntry* btb = &statePtr->btBuf[statePtr->btBufidx];
  btb->branchPC = branchPC;
  btb->branchTarget = targetPC;
  btb->valid = 1;
  btb->branchPred = (outcome == 1) ? STRONGTAKEN : STRONGNOT;

  statePtr->btBufidx = (statePtr->btBufidx + 1) % BTBSIZE;
}

/* 选作内容 */
int getTarget(machineState *statePtr, int reorderNum)
{
  /*
   * [TOD]
   * 检查分支指令是否已保存在分支预测缓冲栈中:
   * 如果不是, 返回当前pc+1, 这意味着我们预测分支跳转不会成功;
   * 如果在, 并且历史信息为STRONGTAKEN或WEAKTAKEN, 返回跳转的目标地址,
   * 如果历史信息为STRONGNOT或WEAKNOT, 返回当前pc+1.
   */
  int nextpc = getPrediction(statePtr);
  if(nextpc == -1){
    return statePtr -> pc + 1;
  }else{
    return nextpc;
  }

}

int main(int argc, char *argv[])
{
  FILE *filePtr;
  int pc, done, instr, i;
  char line[MAXLINELENGTH];
  machineState *statePtr;
  int memorySize;
  int success, newBuf, op, halt, unit;
  int headRB, tailRB;
  int regA, regB, immed, address;
  int flush;
  int rbnum;

  if (argc != 2)
  {
    printf("error: usage: %s <machine-code file>\n", argv[0]);
    exit(1);
  }

  filePtr = fopen(argv[1], "r");
  if (filePtr == NULL)
  {
    printf("error: can't open file %s", argv[1]);
    perror("fopen");
    exit(1);
  }

  /*
   * 初始化, 读输入文件等
   *
   */

  /*
   * 分配数据结构空间
   */

  statePtr = (machineState *)malloc(sizeof(machineState));

  /*
   * 将机器指令读入到内存中
   */

  for (i = 0; i < MEMSIZE; i++)
  {
    statePtr->memory[i] = 0;
  }
  pc = 16;
  done = 0;
  while (!done)
  {
    if (fgets(line, MAXLINELENGTH, filePtr) == NULL)
    {
      done = 1;
    }
    else
    {
      if (sscanf(line, "%d", &instr) != 1)
      {
        printf("error in reading address %d\n", pc);
        exit(1);
      }

      statePtr->memory[pc] = instr;
      printf("memory[%d] = %d\n", pc, statePtr->memory[pc]);
      pc = pc + 1;
    }
  }

  memorySize = pc;
  halt = 0;

  /*
   * 状态初始化
   */

  statePtr->pc = 16;
  statePtr->cycles = 0;
  statePtr->btBufidx = 0;
  for (i = 0; i < NUMREGS; i++)
  {
    statePtr->regFile[i] = 0;
  }
  for (i = 0; i < NUMUNITS; i++)
  {
    statePtr->reservation[i].busy = 0;
  }
  for (i = 0; i < RBSIZE; i++)
  {
    statePtr->reorderBuf[i].busy = 0;
  }

  headRB = 0;
  tailRB = -1;

  for (i = 0; i < NUMREGS; i++)
  {
    statePtr->regResult[i].valid = 1;
  }
  for (i = 0; i < BTBSIZE; i++)
  {
    statePtr->btBuf[i].valid = 0;
  }

  /*
   * 处理指令
   */

  while (1)
  { /* 执行循环:你应该在执行halt指令时跳出这个循环 */

    printState(statePtr, memorySize);

    /*
     * []
     * 基本要求:
     * 首先, 确定是否需要清空流水线或提交位于ROB的队首的指令.
     * 我们处理分支跳转的缺省方法是假设跳转不成功, 如果我们的预测是错误的,
     * 就需要清空流水线(ROB/保留栈/寄存器状态), 设置新的pc = 跳转目标.
     * 如果不需要清空, 并且队首指令能够提交, 在这里更新状态:
     *     对寄存器访问, 修改寄存器;
     *     对内存写操作, 修改内存.
     * 在完成清空或提交操作后, 不要忘了释放保留栈并更新队列的首指针.
     */
    reorderEntry *rob = &statePtr->reorderBuf[headRB];
    if (rob->busy && rob->valid)
    {
      printf("%d\n",rob->pc);
      rob->busy = 0;
      if (opcode(rob->instr) == BEQZ)
      {
        updateBTB(statePtr, rob->pc, rob->result, rob->addValid);
        if (rob->realBeqAddress != (rob->addValid == 1 ? rob->result : (rob->pc + 1)))
        {
          for (int i = 0; i < NUMUNITS; i++)
          {
            statePtr->reservation[i].busy = 0;
          }
          for (int i = 0; i < RBSIZE; i++)
          {
            statePtr->reorderBuf[i].busy = 0;
          }
          for (int i = 0; i < NUMREGS; i++)
          {
            statePtr->regResult[i].valid = 1;
          }
          statePtr->pc = (rob->addValid == 1 ? rob->result : (rob->pc + 1));
          headRB = 0; //?
          tailRB = -1;
        }
        else
        {
          headRB = (headRB + 1) % RBSIZE;
        }
      }
      else if (opcode(rob->instr) == J)
      {
        updateBTB(statePtr, rob->pc, rob->result, 1);
        if(rob->realBeqAddress != rob -> result){
          for (int i = 0; i < NUMUNITS; i++)
          {
            statePtr->reservation[i].busy = 0;
          }
          for (int i = 0; i < RBSIZE; i++)
          {
            statePtr->reorderBuf[i].busy = 0;
          }
          for (int i = 0; i < NUMREGS; i++)
          {
            statePtr->regResult[i].valid = 1;
          }
          statePtr->pc = rob->result;
          headRB = 0; //?
          tailRB = -1;
        }else
        {
          headRB = (headRB + 1) % RBSIZE;
        }
      }
      else if (opcode(rob->instr) == HALT)
      {
        break;
      }
      else
      {
        if (opcode(rob->instr) == SW)
        {
          statePtr->memory[rob->storeAddress] = rob->result;
        }
        else if (opcode(rob->instr) == LW || opcode(rob->instr) == ADDI || opcode(rob->instr) == ANDI)
        {
          int rd = field1(rob->instr);
          statePtr->regFile[rd] = rob->result;
          if (statePtr->regResult[rd].reorderNum == headRB)
          {
            statePtr->regResult[rd].valid = 1;
          }
        }
        else if (opcode(rob->instr) == regRegALU)
        {
          int rd = field2(rob->instr);
          statePtr->regFile[rd] = rob->result;
          if (statePtr->regResult[rd].reorderNum == headRB)
          {
            statePtr->regResult[rd].valid = 1;
          }
        }
        headRB = (headRB + 1) % RBSIZE;
      }
    }

    /*
     * [TOD]
     * 选作内容:
     * 在提交的时候, 我们知道跳转指令的最终结果.
     * 有三种可能的情况: 预测跳转成功, 预测跳转不成功, 不能预测(因为分支预测缓冲栈中没有对应的项目).
     * 如果我们预测跳转成功:
     *     如果我们的预测是正确的, 只需要继续执行就可以了;
     *     如果我们的预测是错误的, 即实际没有发生跳转, 就必须重新设置正确的PC值, 并清空流水线.
     * 如果我们预测跳转不成功:
     *     如果预测是正确的, 继续执行;
     *     如果预测是错误的, 即实际上发生了跳转, 就必须将PC设置为跳转目标, 并清空流水线.
     * 如果我们不能预测跳转是否成功:
     *     如果跳转成功, 仍然需要清空流水线, 将PC修改为跳转目标.
     * 在遇到分支时, 需要更新分支预测缓冲站的内容.
     */

    /*
     * []
     * 提交完成.
     * 检查所有保留栈中的指令, 对下列状态, 分别完成所需的操作:
     */

    /*
     * []
     * 对Writing Result状态:
     * 将结果复制到正在等待该结果的其他保留栈中去;
     * 还需要将结果保存在ROB中的临时存储区中.
     * 释放指令占用的保留栈, 将指令状态修改为Committing
     */
    for (int i = 0; i < NUMUNITS; i++)
    {
      resStation *sta = &statePtr->reservation[i];
      if (sta->busy == 0)
        continue;
      reorderEntry *reo = &statePtr->reorderBuf[sta->reorderNum];
      if (reo->instrStatus == WRITINGRESULT)
      {
        reo->valid = 1;
        int result = getResult(*sta, statePtr);
        updateRes(i, statePtr, result);
        reo->result = result;
        reo->instrStatus = COMMITTING;
        sta->busy = 0;
      }
    }
    /*
     * []
     * 对Executing状态:
     * 执行剩余时间递减;
     * 在执行完成时, 将指令状态修改为Writing Result
     */
    for (int i = 0; i < NUMUNITS; i++)
    {
      resStation *sta = &statePtr->reservation[i];
      if (sta->busy == 0)
        continue;
      reorderEntry *reo = &statePtr->reorderBuf[sta->reorderNum];
      if (reo->instrStatus == EXECUTING)
      {
        sta->exTimeLeft -= 1;
        if (sta->exTimeLeft == 0)
        {
          reo->instrStatus = WRITINGRESULT;
        }
      }
    }

    /*
     * []
     * 对Issuing状态:
     * 检查两个操作数是否都已经准备好, 如果是, 将指令状态修改为Executing
     */
    for (int i = 0; i < NUMUNITS; i++)
    {
      resStation *sta = &statePtr->reservation[i];
      if (sta->busy == 0)
        continue;
      reorderEntry *reo = &statePtr->reorderBuf[sta->reorderNum];
      if (reo->instrStatus == ISSUING)
      {
        if (sta->Qk == 0 && sta->Qj == 0)
        {
          reo->instrStatus = EXECUTING;
        }
      }
    }

    /*
     * []
     * 最后, 当我们处理完了保留栈中的所有指令后, 检查是否能够发射一条新的指令.
     * 首先检查是否有空闲的保留栈, 如果有, 再检查ROB中是否有空闲的空间,
     * 如果也能找到空闲空间, 发射指令.
     */
    int instr = statePtr->memory[statePtr->pc];
    if (instr != 0)
    {
      switch (opcode(instr))
      {
      case regRegALU:
      case ADDI:
      case ANDI:
      case HALT:
      case NOOP:
        if (statePtr->reservation[INT1 - 1].busy == 0)
        {
          int reo;
          if ((reo = checkReorder(statePtr, &headRB, &tailRB)) != -1)
          {
            issueInstr(instr, INT1 - 1, statePtr, reo);
            statePtr->pc += 1; 
          }
        }
        else if (statePtr->reservation[INT2 - 1].busy == 0)
        {
          int reo;
          if ((reo = checkReorder(statePtr, &headRB, &tailRB)) != -1)
          {
            issueInstr(instr, INT2 - 1, statePtr, reo);
            statePtr->pc += 1; 
          }
        }
        break;
      case BEQZ:
      case J:
        if (statePtr->reservation[INT1 - 1].busy == 0)
        {
          int reo;
          if ((reo = checkReorder(statePtr, &headRB, &tailRB)) != -1)
          {
            issueInstr(instr, INT1 - 1, statePtr, reo);
            // statePtr->pc += 1; 
            statePtr->pc = getTarget(statePtr, 0);
            statePtr->reorderBuf[reo].realBeqAddress = statePtr->pc;
          }
        }
        else if (statePtr->reservation[INT2 - 1].busy == 0)
        {
          int reo;
          if ((reo = checkReorder(statePtr, &headRB, &tailRB)) != -1)
          {
            issueInstr(instr, INT2 - 1, statePtr, reo);
            // statePtr->pc += 1; 
            statePtr->pc = getTarget(statePtr, 0);
            statePtr->reorderBuf[reo].realBeqAddress = statePtr->pc;
          }
        }
        break;
      case LW:
        if (statePtr->reservation[LOAD1 - 1].busy == 0)
        {
          int reo;
          if ((reo = checkReorder(statePtr, &headRB, &tailRB)) != -1)
          {
            issueInstr(instr, LOAD1 - 1, statePtr, reo);
            statePtr->pc += 1; 
          }
        }
        else if (statePtr->reservation[LOAD2 - 1].busy == 0)
        {
          int reo;
          if ((reo = checkReorder(statePtr, &headRB, &tailRB)) != -1)
          {
            issueInstr(instr, LOAD2 - 1, statePtr, reo);
            statePtr->pc += 1; 
          }
        }
        break;
      case SW:
        if (statePtr->reservation[STORE1 - 1].busy == 0)
        {
          int reo;
          if ((reo = checkReorder(statePtr, &headRB, &tailRB)) != -1)
          {
            issueInstr(instr, STORE1 - 1, statePtr, reo);
            statePtr->pc += 1; 
          }
        }
        else if (statePtr->reservation[STORE2 - 1].busy == 0)
        {
          int reo;
          if ((reo = checkReorder(statePtr, &headRB, &tailRB)) != -1)
          {
            issueInstr(instr, STORE2 - 1, statePtr, reo);
            statePtr->pc += 1; 
          }
        }
        break;
      default:
        break;
      }
    }

    /*
     * [TOD]
     * 选作内容:
     * 在发射跳转指令时, 将PC修改为正确的目标: 是pc = pc+1, 还是pc = 跳转目标?
     * 在发射其他的指令时, 只需要设置pc = pc+1.
     */

    /*
     * []
     * 周期计数加1
     */
    statePtr->cycles += 1;

    // debug
    //  if (statePtr->cycles > 1000)
    //  {
    //    break;
    //    /* code */
    //  }

  } /* while (1) */
  printf("halting machine\n");
}