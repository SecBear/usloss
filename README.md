# USLOSS (Unix Software Library for Operating System Simulation)
**USLOSS (Unix Software Library for Operating System Simulation)**
is a set of functions that simulate the basic hardware features of a hypothetical computer system. 

The purpose of the simulator is to allow users to experiment with low-level systems programming concepts such as interrupt handling, device drivers, and process scheduling. 

The simulator is written in the C programming language to allow fast execution and easy debugging of programs. 

The simulator provides the appearance of a dedicated, single CPU system. A high-level context switch operation is provided to allow easy switching between processes.
A simulated interrupt system is controlled through a user defined interrupt vector table and through functions that enable and disable interrupts. 

The following interrupts/devices are supported: 
- a periodic clock interrupt
- a system call function
- four user terminals
- two disk storage devices
  
The simulator also supports a user mode and a kernel mode.
