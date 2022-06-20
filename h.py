from tkinter import *
 
def data():
    for i in range(100):
       Label(rollFrame,text=i).grid(row=i,column=0)
       Label(rollFrame,text="my text"+str(i)).grid(row=i,column=1)
       Label(rollFrame,text="..........").grid(row=i,column=2)
 
# 少了这个就滚动不了
def myfunction(event):
    canvas.configure(scrollregion=canvas.bbox("all"),width=200,height=200)
 
 
root=Tk()
root.wm_geometry("800x600")
 
 
canvas=Canvas(root)     # 创建画布
canvas.place(x=0,y=0,height=300,width=500)
 
myscrollbar=Scrollbar(root,orient="vertical",command=canvas.yview)      #创建滚动条
myscrollbar.place(x=500,y=0,height=300)
canvas.configure(yscrollcommand=myscrollbar.set)
 
 
rollFrame=Frame(canvas)     # 在画布上创建frame
canvas.create_window((0,0),window=rollFrame,anchor='nw')    # 要用create_window才能跟随画布滚动
rollFrame.bind("<Configure>",myfunction)
 
 
data()
root.mainloop()
