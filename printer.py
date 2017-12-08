# coding: utf-8
# GDB用のpretty-printer
import lubee.printer
import re
import gdb

def buildPrinter(obj):
    obj.pretty_printers.append(Lookup)
    lubee.printer.buildPrinter(obj)

Re_Spine = re.compile("^spi::(.+)$")
def Lookup(val):
    name = val.type.strip_typedefs().name
    if name == None:
        return None
    obj = Re_Spine.match(name)
    if obj:
        return LookupSpine(obj.group(1), val)
    return None

Re_Opt = re.compile("^Optional<(.+)>$");
def LookupSpine(name, val):
    if Re_Opt.match(name):
        return Optional(val)
    return None

class Iterator:
    def next(self):
        return self.__next__()
class Optional:
    class _iterator(Iterator):
        def __init__(self, inner):
            self._inner = inner
            self._bOutput = False
        def __iter__(self):
            return self
        def __next__(self):
            if self._bOutput:
                raise StopIteration
            self._bOutput = True
            ret = self._inner
            return ("data", ret if ret else gdb.parse_and_eval("::spi::none"))

    def __init__(self, val):
        self._val = val
    def getInner(self):
        if self._val["_bInit"]:
            arg_t = self._val.type.template_argument(0)
            dataptr = self._val["_buffer"]["_data"]
            if not arg_t.code == gdb.TYPE_CODE_PTR and\
                not arg_t.code == gdb.TYPE_CODE_REF:
                dataptr = dataptr.reinterpret_cast(arg_t.pointer())
            return dataptr.dereference()
        return None
    def to_string(self):
        return "(optional)" + self._val.type.template_argument(0).tag
    def children(self):
        return self._iterator(self.getInner())
    def display_hint(self):
        return "string"
