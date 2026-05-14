import os
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("-f", "--files", help = "specify the directory where instances exist", default = os.getcwd())
parser.add_argument("-o", "--output_folder", help = "specify output folder", default = os.getcwd())
parser.add_argument("-m", "--method", help = "specify the method")
parser.add_argument("-s", "--sizeLimit", help = "specify size limit for CS", default = 3)
parser.add_argument("-l", "--labelLimit", help = "specify label limit for LL", default = 5)

flags = parser.parse_args()

if not os.path.exists(flags.output_folder):
    os.makedirs(flags.output_folder)
    
for file in os.listdir(os.path.join(os.getcwd(),flags.files)):
     filename = os.fsdecode(file)
     if filename.endswith(".py"):
        cmd = 'python3 main.py -m {} -i {} -o {} -s {} -l {}'.format(flags.method,
                                                         flags.files + '.' + filename.split('.py')[0],
                                                         flags.output_folder,
                                                         flags.sizeLimit,
                                                         flags.labelLimit)
        print(cmd)
        proc_result = subprocess.run(cmd, shell = True)