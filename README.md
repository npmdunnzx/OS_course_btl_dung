# OS_course_btl
School project ( do not look in :> )

# RUN
#################################

if window system: install makefile in your WSL:

sudo apt update

sudo apt install make

#################################

then: make os (or any below)

make all (do not)

make syscalltbl.lst	(included in make os)

make mem (fail, old module)

make sched (fail)

make os

make clean

run: ./os name_in_input_folder (ex: ./os os_1_mlq_paging_small_4K)

depend on what you make

#################################

example of full run: make clean -> make os -> ./os os_1_mlq_paging_small_4K

#################################

Linux: same
