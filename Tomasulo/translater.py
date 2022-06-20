import sys

address = 0
def error(stri):
    print("error: "+stri)
    print("address: "+str(address))
    exit(0)

def reg(str):
    reglist = ["r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r10","r11","r12","r13","r14","r15","r16","r17","r18","r19","r20","r21","r22","r23","r24","r25","r26","r27","r28","r29","r30","r31"]
    if not str in reglist:
        print("error reg: "+str)
        print("address: "+str(address))
        exit(0)
    return int(str[1:])

def true_int_str(num):
    if num & (1 << 31):
        num -= (1 << 32)
    return str(num)


if len(sys.argv) != 3:
    print("argv error, need 2 args")
    exit(0)

src_file_path = sys.argv[1]
dst_file_path = sys.argv[2]

src_file = open(src_file_path, mode='r')
code_line = src_file.readlines()
src_file.close()

instr_list = ["lw","sw","add","addi","sub","and","andi","beqz","j","halt","noop"]
instr_list_with_3arg = ["lw","sw","add","addi","sub","and","andi"]
instr_map_R_fun = {"add":32,"sub":34,"and":36}
instr_map_I_op = {"lw":35,"sw":43,"andi":12,"addi":8}
label_map = {}

# first pass for label
address = 0
for line in code_line:
    if line.find(";") != -1:
        line = line[:line.find(";")]  
    word_list = [s for s in line.split() if s]

    word_index = 0
    while word_index < len(word_list):
        word = word_list[word_index]
        if word in instr_list:
            if word == "beqz":
                word_index += 1
                arg_list = [s for s in word_list[word_index].split(",") if s]
                if(len(arg_list)) == 1:
                    word_index +=1
            elif word == "j":
                word_index += 1
            elif word in instr_list_with_3arg:
                num = 3
                while num > 0:
                    word_index += 1
                    arg_list = [s for s in word_list[word_index].split(",") if s]
                    num -= len(arg_list)
            address += 1
        else:
            if word in label_map.keys():
                error("reuse label: "+word)
            label_map[word] = address
        word_index += 1
            

dst_file = open(dst_file_path, mode='w')

# second pass for translate
address = 0
for line in code_line:
    if line.find(";") != -1:
        line = line[:line.find(";")]  
    word_list = [s for s in line.split() if s]

    word_index = 0
    while word_index < len(word_list):
        word = word_list[word_index]
        if word in instr_list:
            if word == "halt":
                dst_file.write(true_int_str(1<<26)+'\n')
            elif word == "noop":
                dst_file.write(true_int_str(3<<26)+'\n')
            elif word == "beqz":
                instr = 0
                instr |= 4 << 26
                word_index += 1
                arg_list = [s for s in word_list[word_index].split(",") if s]
                if(len(arg_list)) == 1:
                    instr |= reg(arg_list[0]) << 21
                    word_index += 1
                    arg_list = [s for s in word_list[word_index].split(",") if s]
                    if(len(arg_list)) != 1:
                        error(word+" "+arg_list[0])
                    else:
                        instr |= ((label_map[arg_list[0]] - address - 1) & 0xffff)
                elif(len(arg_list)) == 2:
                    instr |= reg(arg_list[0]) << 21
                    instr |= (label_map[arg_list[1]] - address - 1)
                else:
                    error(word+" "+arg_list[0])
                dst_file.write(true_int_str(instr)+'\n')
            elif word == "j":
                instr = 0
                instr |= 2 << 26
                word_index += 1
                arg_list = [s for s in word_list[word_index].split(",") if s]
                if(len(arg_list)) == 1:
                    instr |= ((label_map[arg_list[0]] - address - 1) & 0x3ffffff)
                else:
                    error(word+" "+arg_list[0])
                dst_file.write(true_int_str(instr)+'\n')
            elif word in instr_map_R_fun.keys():
                instr = 0
                instr |= instr_map_R_fun[word]
                num = 3
                all_list=[]
                while num > 0:
                    word_index += 1
                    arg_list =[s for s in word_list[word_index].split(",") if s]
                    num -= len(arg_list)
                    if num < 0:
                        error(word+" "+arg_list[0])
                    all_list += arg_list
                instr |= (reg(all_list[0]) << 11)
                instr |= (reg(all_list[1]) << 21)
                instr |= (reg(all_list[2]) << 16)
                dst_file.write(true_int_str(instr)+'\n')
            elif word in instr_map_I_op.keys():
                instr = 0
                instr |= (instr_map_I_op[word] << 26)
                num = 3
                all_list=[]
                while num > 0:
                    word_index += 1
                    arg_list = [s for s in word_list[word_index].split(",") if s]
                    num -= len(arg_list)
                    if num < 0:
                        error(word+" "+arg_list[0])
                    all_list += arg_list
                instr |= (reg(all_list[0]) << 16)
                instr |= (reg(all_list[1]) << 21)
                instr |= (int(all_list[2]) & 0xffff)
                dst_file.write(true_int_str(instr)+'\n')
            address += 1
        else:
            pass
        word_index += 1

dst_file.close()
