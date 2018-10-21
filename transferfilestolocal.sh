#!/bin/bash

echo "Copying .cpp files"
find . -name '*.cpp' | cpio -pdm /home/lihki/Documentos/omnicore-litecoin-local
echo "Done"

echo "Copying .h files"
find . -name '*.h' | cpio -pdm /home/lihki/Documentos/omnicore-litecoin-local
echo "Done"
