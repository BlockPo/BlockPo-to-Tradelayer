a=$(ls | grep tl_)
for i in ${a}
do
./$i
done
