set can-use-hw-watchpoints 0
define asst3
dir kern/compile/ASST3
target remote unix:.sockets/gdb
b panic
end
