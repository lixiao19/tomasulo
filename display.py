from logging import root
import re
from tkinter import *
from tkinter.ttk import Treeview
import sys

class Reservation:
    def __init__(self, idx, busy=0, Vj="", Qj="", Vk="", Qk="", exTimeLeft="", reorder=""):
        self.idx = idx
        self.busy = busy
        self.Vj = Vj
        self.Qj = Qj
        self.Vk = Vk
        self.Qk = Qk
        self.exTimeLeft = exTimeLeft
        self.reorder = reorder
    
    def message_list(self):
        uintmap = {0:"LOAD1",1:"LOAD2",2:"STORE1",3:"STORE2",4:"INT1",5:"INT2"}
        return [uintmap[self.idx], self.busy, self.Vj, self.Qj, self.Vk, self.Qk, self.exTimeLeft, self.reorder]
    
    def title_list():
        return ["执行单元","busy","Vj","Qj","Vk","Qk","剩余时间","对应ROB"]

class Rob:
    def __init__(self, idx, busy = 0, instr = "", execUnit = "", instrStatus = "", valid = "", result = "", storeAddress = "") -> None:
        self.idx = idx
        self.busy = busy
        self.instr = instr
        self.execUnit = execUnit
        self.instrStatus = instrStatus
        self.valid = valid
        self.result = result
        self.storeAddress = storeAddress
    
    def message_list(self):
        return [self.idx, self.busy, self.instr, self.execUnit, self.instrStatus, self.valid, self.result, self.storeAddress]

    def title_list():
        return ["序号","busy","指令","执行单元","指令状态","结果有效性","结果","访存地址"] 

class Btb:
    def __init__(self, idx, branchpc = "", branchtarget="", branchpred="") -> None:
        self.idx = idx
        self.branchpc = branchpc
        self.branchtarget = branchtarget
        self.branchpred = branchpred
    
    def message_list(self):
        return [self.idx, self.branchpc, self.branchtarget, self.branchpred]
    
    def title_list():
        return ["序号","pc","目标pc","状态"]


class Register_list:
    def __init__(self) -> None:
        self.value = [0 for i in range(32)]
        self.waitrob = ["" for i in range(32)]
    
    def value_list(self):
        return self.value
    
    def waitrob_list(self):
        return self.waitrob
    
    def title_list():
        return ["r"+str(i) for i in range(32)]

class Memory:
    def __init__(self) -> None:
        self.memory_list = [0 for i in range(16)]
    
    def get_memory(self):
        return self.memory_list
    
    def title_list():
        return [hex(i) for i in range(16)]

class Instr_list():
    def __init__(self) -> None:
        self.instr_list = []
        self.now_add = 16
    
    def get_instr_list(self):
        return self.instr_list

class Cycle:
    def __init__(self, idx):
        self.idx = idx
    def set_pc(self, pc):
        self.pc = pc
    def set_reservation_list(self, reservation_list):
        self.reservation_list = reservation_list
    def set_roob_list(self, rob_list):
        self.rob_list = rob_list
    def set_register_list(self, register_list: Register_list):
        self.register_list = register_list
    def set_memory_list(self, memory_list: Memory):
        self.memory_list = memory_list
    def set_btb_list(self, btb_list):
        self.btb_list = btb_list
    

def instr_trans(instr_value):
    instr = ""
    ins_I = [35,43,8,12]
    opcode_map = {35:"LW", 43:"SW", 0:"", 8:"ADDI", 12:"ANDI", 4:"BEQZ", 2:"J", 1:"HALT", 3:"NOOP"}
    func_map = {32:"ADD", 34:"SUB", 36:"AND"}
    opcode = (instr_value >> 26) & 0x3F
    op = ""
    if opcode == 0:
        op = func_map[instr_value & 0x7FF]
        instr += op
        instr += " "
        instr += ("r" + str((instr_value >> 11) & 0x1F) + ",")
        instr += ("r" + str((instr_value >> 21) & 0x1F) + ",")
        instr += ("r" + str((instr_value >> 16) & 0x1F))
    elif opcode == 1 or opcode == 3:
        instr += opcode_map[opcode]
    elif opcode in ins_I:
        instr += opcode_map[opcode]
        instr += " "
        instr += ("r" + str((instr_value >> 16) & 0x1F) + ",")
        instr += ("r" + str((instr_value >> 21) & 0x1F) + ",")
        imm = instr_value & 0xFFFF
        if (instr_value >> 15) & 1:
            imm = (1 << 16) - imm
            imm = -imm
        instr += str(imm)
    elif opcode == 4:
        instr += opcode_map[opcode]
        instr += " "
        instr += ("r" + str((instr_value >> 21) & 0x1F) + ",")
        imm = instr_value & 0xFFFF
        if (instr_value >> 15) & 1:
            imm = (1 << 16) - imm
            imm = -imm
        instr += str(imm)
    elif opcode == 2:
        instr += opcode_map[opcode]
        instr += " "
        imm = instr_value & 0x3FFFFFF
        if (instr_value >> 25) & 1:
            imm = (1 << 26) - imm
            imm = -imm
        instr += str(imm)
    return instr


def read(cycle_list, instr_list, src_file_name):
    r_file = open(src_file_name, mode='r')
    message_lines = r_file.readlines()
    cycle = Cycle(0)
    for line_idx in range(len(message_lines)):
        line = message_lines[line_idx]

        if("Cycles:" in line):
            cycle = Cycle(line[8:-1])
            reser_list = []
            rob_list = []
            btb_list = []
            for i in range(6):
                reser_list.append(Reservation(i))
            cycle.set_reservation_list(reser_list)
            for i in range(16):
                rob_list.append(Rob(i))
            cycle.set_roob_list(rob_list)
            for i in range(8):
                btb_list.append(Btb(i))
            cycle.set_btb_list(btb_list)
            cycle.set_register_list(Register_list())
            cycle.set_memory_list(Memory())
            cycle_list.append(cycle)
        if("pc=" in line):
            pcM = re.search(r'pc=(.*)', line)
            pc = pcM.group(1)
            cycle.set_pc(pc)
        if("Reservation station " in line):
            res_id = int(re.search(r"Reservation station (\S*):", line).group(1))
            cycle.reservation_list[res_id].busy = 1
            Vjm = re.search(r"Vj = (\S*)", line)
            if Vjm != None:
                cycle.reservation_list[res_id].Vj = Vjm.group(1)
            Qjm = re.search(r"Qj = '(\S*)'", line)
            if Qjm != None:
                cycle.reservation_list[res_id].Qj = Qjm.group(1)
            Vkm = re.search(r"Vk = (\S*)", line)
            if Vkm != None:
                cycle.reservation_list[res_id].Vk = Vkm.group(1)
            Qkm = re.search(r"Qk = '(\S*)'", line)
            if Qkm != None:
                cycle.reservation_list[res_id].Qk = Qkm.group(1)
            timem = re.search(r"ExTimeLeft = (\S*)", line)
            if timem != None:
                cycle.reservation_list[res_id].exTimeLeft = timem.group(1)
            ROBm = re.search(r"RBNum = (\S*)", line)
            if ROBm != None:
                cycle.reservation_list[res_id].reorder = ROBm.group(1)
        if "Reorder buffer " in line:
            rob_id = int(re.search(r"Reorder buffer (\S*):", line).group(1))
            cycle.rob_list[rob_id].busy = 1
            instrm = re.search(r"instr (\S*)", line)
            if instrm != None:
                cycle.rob_list[rob_id].instr = instr_trans(int(instrm.group(1)))
            executionUnitm = re.search(r"executionUnit '(\S*)'", line)
            if executionUnitm != None:
                cycle.rob_list[rob_id].execUnit = executionUnitm.group(1)
            statem = re.search(r"state (\S*)", line)
            if statem != None:
                cycle.rob_list[rob_id].instrStatus = statem.group(1)
            validm = re.search(r"valid (\S*)", line)
            if validm != None:
                cycle.rob_list[rob_id].valid = validm.group(1)
            resultm = re.search(r"result (\S*)", line)
            if resultm != None:
                cycle.rob_list[rob_id].result = resultm.group(1)
            storeAddressm = re.search(r"storeAddress (\S*)", line)
            if storeAddressm != None:
                cycle.rob_list[rob_id].storeAddress = storeAddressm.group(1)
        if "Register " in line and "Register result status" not in line:
            reg_id = int(re.search(r"Register (\S*):", line).group(1))
            wait_rob = re.search(r"waiting for reorder buffer number (\S*)", line).group(1)
            cycle.register_list.waitrob[reg_id] = wait_rob
        if "regFile[" in line:
            reg_id = int(re.search(r"regFile\[(\S*)\]", line).group(1))
            value = int(re.search(r"regFile\[\S*\] = (\S*)", line).group(1))
            cycle.register_list.value[reg_id] = value
        if "memory[" in line:
            mem_id = int(re.search(r"memory\[(\S*)\]", line).group(1))
            value = int(re.search(r"memory\[\S*\] = (\S*)", line).group(1))
            if mem_id < 16:
                cycle.memory_list.memory_list[mem_id] = value
            elif mem_id == instr_list.now_add:
                instr_list.instr_list.append(instr_trans(value))
                instr_list.now_add += 1
        if "Entry " in line:
            btb_id = int(re.search(r"Entry (\S*): PC=(\S*), Target=(\S*), Pred=(\S*)", line).group(1))
            pc = re.search(r"Entry (\S*): PC=(\S*), Target=(\S*), Pred=(\S*)", line).group(2)
            target_pc = re.search(r"Entry (\S*): PC=(\S*), Target=(\S*), Pred=(\S*)", line).group(3)
            label = int(re.search(r"Entry (\S*): PC=(\S*), Target=(\S*), Pred=(\S*)", line).group(4))
            cycle.btb_list[btb_id].branchpc = pc
            cycle.btb_list[btb_id].branchtarget = target_pc
            cycle.btb_list[btb_id].branchpred = str(label >> 1 & 1) + str(label & 1)
    r_file.close()

    
if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("argv error, need 1 args")
        exit(0)
    src_file_name = sys.argv[1]
    cycle_list  = [] 
    instr_list = Instr_list()
    read(cycle_list, instr_list, src_file_name)

    cycle_idx = 0

    rot = Tk()
    rot.wm_geometry("850x940")

    sbar1= Scrollbar(rot)
    # 将滚动条放置在右侧，并设置当窗口大小改变时滚动条会沿着垂直方向延展
    sbar1.pack(side=RIGHT, fill=Y)

    canvas = Canvas(rot, yscrollcommand=sbar1.set)
    sbar1.config(command=canvas.yview)
    canvas.pack(fill=BOTH)
    # canvas.place(x=0,y=0,height=800,width=1000)
    

    def myfunction(event):
        canvas.configure(scrollregion=canvas.bbox("all"),width=850,height=940)

    root = Frame(canvas)
    canvas.create_window((0,0),window=root,anchor='nw')
    root.bind("<Configure>",myfunction)



    left_label = Label(root)
    left_label.pack(side="left")

    cycle_label = Label(left_label, text="cycle: 0\npc: 16", font=("微软雅黑", 30))
    cycle_label.pack()


    instr_label = Label(left_label, text="指令")
    instr_label.pack()

    instr_tree = Treeview(left_label, show="headings", columns=["地址", "指令"], selectmode= BROWSE,height=len(instr_list.get_instr_list()))
    instr_tree.column("地址", width=60, anchor="center")
    instr_tree.column("指令", width=120, anchor="center")
    instr_tree.heading("地址",text="地址")
    instr_tree.heading("指令",text="指令")
    for i in range(len(instr_list.get_instr_list())):
        instr_tree.insert("",i,values=[16+i, instr_list.get_instr_list()[i]])
    instr_tree.pack(padx=10,pady=10)

    pre_label = Label(root, text="保留站")
    pre_label.pack()

    preserv_tree = Treeview(root, show="headings", columns=Reservation.title_list(),selectmode= BROWSE,height=6)
    for i in range(8):
        preserv_tree.column(Reservation.title_list()[i], width=75,anchor="center")
        preserv_tree.heading(Reservation.title_list()[i], text=Reservation.title_list()[i])
    for i in range(6):
        preserv_tree.insert("",i,values=cycle_list[cycle_idx].reservation_list[i].message_list())
    preserv_tree.pack(padx=10,pady=10)

    btb_label = Label(left_label, text="BTB")
    btb_label.pack()

    btb_tree = Treeview(left_label, show="headings", columns=Btb.title_list(),selectmode= BROWSE,height=8)
    for i in range(4):
        btb_tree.column(Btb.title_list()[i], width=45,anchor="center")
        btb_tree.heading(Btb.title_list()[i], text=Btb.title_list()[i])
    for i in range(8):
        btb_tree.insert("",i,values=cycle_list[cycle_idx].btb_list[i].message_list())
    btb_tree.pack(padx=10,pady=10)

    rob_label = Label(root, text="ROB")
    rob_label.pack()

    rob_tree = Treeview(root, show="headings", columns=Rob.title_list(),selectmode= BROWSE,height=16)
    for i in range(8):
        wid = 60
        if i == 2:
            wid = 120
        if i == 4:
            wid = 120
        rob_tree.column(Rob.title_list()[i], width=wid,anchor="center")
        rob_tree.heading(Rob.title_list()[i], text=Rob.title_list()[i])
    for i in range(16):
        rob_tree.insert("",i,values=cycle_list[cycle_idx].rob_list[i].message_list())
    rob_tree.pack(padx=10,pady=10)

    reg_name_label = Label(root, text="寄存器")
    reg_name_label.pack()

    reg_label = Label(root)
    reg_label.pack(padx=10,pady=10)

    reg_tree1 = Treeview(reg_label, show="headings", columns=([" "]+Register_list.title_list()[0:8]),selectmode= BROWSE,height=2)
    reg_tree2 = Treeview(reg_label, show="headings", columns=([" "]+Register_list.title_list()[8:16]),selectmode= BROWSE,height=2)
    reg_tree3 = Treeview(reg_label, show="headings", columns=([" "]+Register_list.title_list()[16:24]),selectmode= BROWSE,height=2)
    reg_tree4 = Treeview(reg_label, show="headings", columns=([" "]+Register_list.title_list()[24:32]),selectmode= BROWSE,height=2)
    def init_reg_tree(reg_tree, idx):
        for i in range(9):
            reg_tree.column(([" "]+Register_list.title_list()[idx:idx+8])[i], width=66,anchor="center")
            reg_tree.heading(([" "]+Register_list.title_list()[idx:idx+8])[i], text=(["reg"]+Register_list.title_list()[idx:idx+8])[i])
        reg_tree.insert("",0,values=(["值"]+cycle_list[cycle_idx].register_list.value_list()[idx:idx+8]))
        reg_tree.insert("",1,values=(["ROB"]+cycle_list[cycle_idx].register_list.waitrob_list()[idx:idx+8]))

    def update_reg_tree(reg_tree, idx):
        for o in reg_tree.get_children():
            reg_tree.delete(o)
        reg_tree.insert("",0,values=(["值"]+cycle_list[cycle_idx].register_list.value_list()[idx:idx+8]))
        reg_tree.insert("",1,values=(["ROB"]+cycle_list[cycle_idx].register_list.waitrob_list()[idx:idx+8]))

    init_reg_tree(reg_tree1, 0)
    init_reg_tree(reg_tree2, 8)
    init_reg_tree(reg_tree3, 16)
    init_reg_tree(reg_tree4, 24)
    reg_tree1.pack()
    reg_tree2.pack()
    reg_tree3.pack()
    reg_tree4.pack()

    mem_name_label = Label(root, text="内存")
    mem_name_label.pack()

    mem_label = Label(root)
    mem_label.pack(padx=10,pady=10)
    mem_tree1 = Treeview(mem_label, show="headings", columns=([" "]+Memory.title_list()[0:8]),selectmode= BROWSE,height=1)
    mem_tree2 = Treeview(mem_label, show="headings", columns=([" "]+Memory.title_list()[8:16]),selectmode= BROWSE,height=1)
    def init_mem_tree(mem_tree, idx):
        for i in range(9):
            mem_tree.column(([" "]+Memory.title_list()[idx:idx+8])[i], width=66,anchor="center")
            mem_tree.heading(([" "]+Memory.title_list()[idx:idx+8])[i], text=(["address"]+Memory.title_list()[idx:idx+8])[i])
        mem_tree.insert("",0,values=(["值"]+cycle_list[cycle_idx].memory_list.get_memory()[idx:idx+8]))

    def update_mem_tree(mem_tree, idx):
        for o in mem_tree.get_children():
            mem_tree.delete(o)
        mem_tree.insert("",0,values=(["值"]+cycle_list[cycle_idx].memory_list.get_memory()[idx:idx+8]))

    init_mem_tree(mem_tree1, 0)
    init_mem_tree(mem_tree2, 8)
    mem_tree1.pack()
    mem_tree2.pack()



    def update(cycle_idx):
        for o in rob_tree.get_children():
            rob_tree.delete(o)
        for i in range(16):
            rob_tree.insert("",i,values=cycle_list[cycle_idx].rob_list[i].message_list())

        for o in preserv_tree.get_children():
            preserv_tree.delete(o)
        for i in range(6):
            preserv_tree.insert("",i,values=cycle_list[cycle_idx].reservation_list[i].message_list())
        
        for o in btb_tree.get_children():
            btb_tree.delete(o)
        for i in range(8):
            btb_tree.insert("",i,values=cycle_list[cycle_idx].btb_list[i].message_list())
        
        update_reg_tree(reg_tree1, 0)
        update_reg_tree(reg_tree2, 8)
        update_reg_tree(reg_tree3, 16)
        update_reg_tree(reg_tree4, 24)

        update_mem_tree(mem_tree1, 0)
        update_mem_tree(mem_tree2, 8)

        if(int(cycle_list[cycle_idx].pc)-16 >= len(instr_tree.get_children())):
            instr_tree.selection_set(instr_tree.get_children()[len(instr_tree.get_children()) - 1])
        else:
            instr_tree.selection_set(instr_tree.get_children()[int(cycle_list[cycle_idx].pc)-16])
        
        
        cycle_label["text"] = "cycle: "+ str(cycle_idx)+"\n pc: "+str(cycle_list[cycle_idx].pc)


    def before(event):
        global cycle_idx
        if cycle_idx > 0:
            cycle_idx = cycle_idx - 1
        update(cycle_idx)

    def after(event):
        global cycle_idx
        if cycle_idx < len(cycle_list) - 1:
            cycle_idx = cycle_idx + 1
        update(cycle_idx)

    rot.bind("<Key-Left>", before)
    rot.bind("<Key-Right>", after)

    rot.focus_set()
    rot.title("Tomasulo算法演示")

    rot.mainloop()