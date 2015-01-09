#!/bin/sh

if [ "$#" != 3 ]; then
    echo "USAGE: $0 LIB_PREFIX BUNDLE EXE";
    exit 1;
fi

prefix=$1
bundle=$2
exe=$3

mkdir -p "$bundle/Contents/lib"

# Replace Control with Command in key bindings
sed -i '' 's/GDK_CONTROL_MASK/GDK_META_MASK/' $bundle/Contents/patchage.ui

# Copy font configuration files
cp $prefix/etc/fonts/fonts.conf $bundle/Contents/Resources

# Copy GTK and pango modules to bundle
mkdir -p "$bundle/Contents/lib/modules"
mkdir -p "$bundle/Contents/lib/gtk-2.0/engines"
cp $prefix/lib/gtk-2.0/2.10.0/engines/libquartz.so  $bundle/Contents/lib/gtk-2.0/engines
cp $prefix/lib/pango/1.8.0/modules/*basic*.so $bundle/Contents/lib/modules

# Copy libraries depended on by the executable to bundle
libs="`otool -L $exe | grep '\.dylib\|\.so' | grep '/User\|/opt/local\|/usr/local' | sed 's/(.*//'`"
for l in $libs; do
	cp $l $bundle/Contents/lib
done

# Add libraries depended on by those libraries
reclibs="`otool -L $bundle/Contents/lib/* $bundle/Contents/lib/gtk-2.0/engines/* $bundle/Contents/lib/modules/* | grep '\.dylib\|\.so' | grep \"$prefix\|/User\|/opt/local\|/usr/local\" | sed 's/(.*//'`"
for l in $reclibs; do
	cp $l $bundle/Contents/lib
done

# ... and libraries depended on by those libraries (yes, this should be done more sanely)
recreclibs="`otool -L $bundle/Contents/lib/* $bundle/Contents/lib/gtk-2.0/engines/* $bundle/Contents/lib/modules/* | grep '\.dylib\|\.so' | grep \"$prefix\|/User\|/opt/local\|/usr/local\" | sed 's/(.*//'`"
for l in $recreclibs; do
	cp $l $bundle/Contents/lib
done

for l in $libs $reclibs $recreclibs; do
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
