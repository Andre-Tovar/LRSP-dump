INSTPATH=/home/saf418/LRP_DipPy/test
OUTPATH=/home/saf418/LRP_DipPy/
for file in ${INSTPATH}/*.py
do
    instance_name=`basename ${file%.*}`
    qsub -N ${instance_name} -l mem=10GB,vmem=10GB -e ${OUTPATH}/${instance_name}.err -o ${OUTPATH}/${instance_name}.out -q long -v FILENAME=test.${instance_name} submitNew.pbs  
done 