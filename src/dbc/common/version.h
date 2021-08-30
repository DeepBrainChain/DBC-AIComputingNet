#ifndef DBC_VERSION_H
#define DBC_VERSION_H

//major version . minor version . revision version . build version
#define CORE_VERSION                        0x00030703
#define PROTOCO_VERSION                     0x00000001

#define STR_CONV(v)  #v
#define STR_VER(v)  STR_CONV(v)

#define SVR_VERSION(x,y,z)   (0x10000*(x) + 0x100*(y) + (z))
#define SVR_VERSION_MAJOR(x) (((x)>>16) & 0xFF)
#define SVR_VERSION_MINOR(x) (((x)>> 8) & 0xFF)
#define SVR_VERSION_PATCH(x) ( (x)      & 0xFF)


#endif //DBC_VERSION_H
