from subprocess import call

progs = [
         ('400.perlbench', 207000000000, './perlbench_base.i386 -I./lib diffmail.pl 4 800 10 17 19 300'),
         ('401.bzip2', 301000000000, './bzip2_base.i386 input.source 280'),
         ('403.gcc', 107000000000, './gcc_base.i386 cp-decl.i -o cp-decl.s'),
         ('429.mcf', 377000000000, './mcf_base.i386 inp.in'),
         ('450.soplex', 364000000000, './soplex_base.i386 -m3500 ref.mps'),
         ('456.hmmer', 264000000000, './hmmer_base.i386 nph3.hmm swiss41'),
         ('471.omnetpp', 43000000000, './omnetpp_base.i386 omnetpp.ini ')
         # ('483.xalancbmk', 1331000000000, './xalancbmk_base.i386 -v t5.xml xalanc.xsl ')
         ]

defines = [
    # (128, 30, 128, 26, 64), # Budget 2kb
    (256, 16, 256, 13, 128), # Budget 2kb
    # (512, 8, 512, 7, 256) # Budget 2kb
    ]

def command(prog, ff, cmd, cnt):
  st = "cd ~/spec_2006/" + prog + "; date > prog.twodate-" + cnt + "; ~/pin-3.0-76991-gcc-linux/pin -t ~/pin-3.0-76991-gcc-linux/source/tools/Project/obj-ia32/Project.so -o " + prog + ".twooutput-" + cnt + " -f " + str(ff) + " -- " + cmd + " > " + prog + ".twostdout-" + cnt + " 2> prog.twoerror-" + cnt + "; date >> prog.twodate-" + cnt
  return "bash -c '" + st + "'"

for prog in progs:
  print(prog[0])
  cnt = 0
  for budget in defines:
    cnt = cnt+1

    # f = open('template.cpp').read()
    # f = f.replace('XXX1', str(budget[0]))
    # f = f.replace('XXX2', str(budget[1]))
    # f = f.replace('XXX3', str(budget[2]))
    # f = f.replace('XXX4', str(budget[3]))
    # f = f.replace('XXX5', str(budget[4]))
    # open('Project.cpp', "w").write(f)

    call(["make", "TARGET=ia32", "clean"])
    call(["make", "TARGET=ia32", "obj-ia32/Project.so"])

    sessname = prog[0].split('.')[1] + "-" + str(cnt)

    call(["tmux", "kill-session", "-t", sessname])
    call(["tmux", "new-session", "-d", "-s", sessname,
      command(prog[0], prog[1], prog[2], str(cnt))])
    call(["sleep", "1"])
