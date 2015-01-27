#
# fbthrift
#
INCLUDEPATH += $${PWD}/thrift/lib/cpp
LIBS += -L$${PWD}/thrift/lib/cpp/.libs
LIBS += -lthriftz
LIBS += -lthrift

#LIBS += $${PWD}/thrift/lib/cpp2/.libs/libthriftcpp2.a
LIBS += -L$${PWD}/thrift/lib/cpp2/.libs
LIBS += -lthriftcpp2

#
# folly
#
include(thrift/folly/folly.pri)
