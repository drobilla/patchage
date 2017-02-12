#!/bin/bash

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

# Copy GTK and pango modules
mkdir -p "$bundle/Contents/lib/modules"
mkdir -p "$bundle/Contents/lib/gtk-2.0/engines"
cp $prefix/lib/gtk-2.0/2.10.0/engines/libquartz.so  $bundle/Contents/lib/gtk-2.0/engines
cp $(find /usr/local/Cellar/pango -name '*basic-coretext*') $bundle/Contents/lib/modules

# Copy GdkPixbuf loaders
mkdir -p $bundle/Contents/lib/gdk-pixbuf-2.0/2.10.0/loaders/
for fmt in icns png; do
   cp $prefix/lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-$fmt.so \
      $bundle/Contents/lib/gdk-pixbuf-2.0/2.10.0/loaders/;
done

chmod -R 755 $bundle/Contents/lib/*

# Copy libraries depended on by the executable to bundle
libs="`otool -L $exe | grep '\.dylib\|\.so' | grep '/User\|/usr/local' | sed 's/(.*//'`"
for l in $libs; do
	cp $l $bundle/Contents/lib/;
done
chmod 755 $bundle/Contents/lib/*

# ... recursively
while true; do
    newlibs=$libs

    # Copy all libraries this library depends on to bundle
	for l in $(find $bundle -name '*.dylib' -or -name '*.so'); do
		reclibs="`otool -L $l | grep '\.dylib\|\.so' | grep '/User\|/usr/local' | sed 's/(.*//'`"
		for rl in $reclibs; do
			cp $rl $bundle/Contents/lib/;
		done
	    chmod 755 $bundle/Contents/lib/*
        newlibs=$(echo "$newlibs"; echo "$reclibs")
	done

    # Exit once we haven't added any new libraries
    newlibs=$(echo "$newlibs" | sort | uniq)
	if [ "$newlibs" = "$libs" ]; then
		break;
	fi
    libs=$newlibs
done

echo "Bundled libraries:"
echo "$libs"

for l in $libs; do
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
