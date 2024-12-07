cl /O2 /c /I. fc.c
cl /O2 /c /I. texta.c
cl /O2 /c /I. textw.c
rc fc.rc
link /out:fc.exe fc.obj texta.obj textw.obj fc.res user32.lib
