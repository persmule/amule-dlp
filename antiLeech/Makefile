objects = antiLeech.o antiLeech_wx.o Interface.o

.PHONY all: antiLeech.so

antiLeech.so: $(objects)
	gcc -shared $(objects) -o antiLeech.so

antiLeech.o:	CString_wx.h antiLeech.h
	$(run-complie)

antiLeech_wx.o:	antiLeech_wx.h
	$(run-complie)

Interface.o:	antiLeech.h
	$(run-complie)

.PHONY clean:
	-rm *.o antiLeech.so

define run-complie
	gcc `wx-config --cppflags` -fPIC -c $(@:.o=.cpp)
endef
