make

echo "Contiguous allocation: " >>results.txt
echo "" >>results.txt

./contiguous $1 >>results.txt

echo "" >>results.txt
echo "Linked allocation: " >>results.txt
echo "" >>results.txt

./linked $1 >>results.txt

echo "" >>results.txt
echo "Done! " >>results.txt
echo "" >>results.txt
echo "Done!"