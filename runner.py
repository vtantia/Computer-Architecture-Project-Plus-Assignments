from subprocess import call

progs = [('400.perlbench', 207000000000, './perlbench_base.i386 -I./lib diffmail.pl 4 800 10 17 19 300'),
         ('401.bzip2', 301000000000, './bzip2_base.i386 input.source 280'),
         ('403.gcc', 107000000000, './gcc_base.i386 cp-decl.i -o cp-decl.s'),
         ('429.mcf', 377000000000, './mcf_base.i386 inp.in'),
         ('450.soplex', 364000000000, './soplex_base.i386 -m3500 ref.mps'),
         ('456.hmmer', 264000000000, './hmmer_base.i386 nph3.hmm swiss41'),
         ('471.omnetpp', 43000000000, './omnetpp_base.i386 omnetpp.ini '),
         ('483.xalancbmk', 1331000000000, './xalancbmk_base.i386 -v t5.xml xalanc.xsl ')]

def command(prog, ff, cmd):
  st = "cd ~/spec_2006/" + prog + "; date > prog.date; ~/pin-3.0-76991-gcc-linux/pin -t ~/pin-3.0-76991-gcc-linux/source/tools/HW1/obj-ia32/HW2.so -o " + prog + ".output -f " + str(ff) + " -- " + cmd + " > " + prog + ".stdout 2> prog.error; date >> prog.date"
  return "bash -c '" + st + "'"

for prog in progs:
  print(prog[0])
  call(["tmux", "kill-session", "-t", prog[0].split('.')[1]])
  call(["tmux", "new-session", "-d", "-s", prog[0].split('.')[1], command(prog[0], prog[1], prog[2])])
