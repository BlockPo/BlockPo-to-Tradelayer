a=$(ls | grep tl_)
forb="tl_tests.sh"
for i in ${a}
do
    if [[ "$i" != "$forb" ]];
    then
        ./$i
    fi
done
