#!/bin/sh

bundle=$1
exe=$2

libs="`otool -L $exe | grep '\.dylib\|\.so' | grep '/User\|/opt/local' | sed 's/(.*//'`"

mkdir "$bundle/Contents/lib"
for l in $libs; do
	cp $l $bundle/Contents/lib
done

mkdir "$bundle/Contents/lib/engines"
cp /opt/local/lib/gtk-2.0/2.10.0/engines/libclearlooks.so $bundle/Contents/lib/engines

mkdir "$bundle/Contents/lib/modules"
cp /opt/local/lib/pango/1.6.0/modules/*basic*.so $bundle/Contents/lib/modules

reclibs="`otool -L $bundle/Contents/lib/engines/* $bundle/Contents/lib/modules/* | grep '\.dylib\|\.so' | grep '/User\|/opt/local' | sed 's/(.*//'`"

for l in $libs $reclibs; do
	lname=`echo $l | sed 's/.*\///'`
	lid="@executable_path/lib/$lname"
	lpath="$bundle/Contents/lib/$lname"
	install_name_tool -id $lid $lpath
	install_name_tool -change $l $lid $exe
	for j in `find $bundle -name '*.so' -or -name '*.dylib'`; do
		install_name_tool -change $l $lid $j
	done;
done

echo "External library references:"
otool -L $exe `find $bundle -name '*.so' -or -name '*.dylib'` | grep -v ':' | grep -v '@executable_path' | sort | uniq
