%modern
%requires LentMod

# qore#7336 x qore#7052: the user constructor of a lent object (a capability) that the code it was lent to calls runs
# as a method of the lent object, also when it is called directly: what it creates of another module's classes is not
# lent (a method of a lent object lends only the objects of its own module's classes)

# a class of the host's Program that inherits a module's class: its objects are capabilities when unrestricted code
# creates them, and its constructor runs in the host's Program
class Holder inherits LentMod::Conn {
    public {
        LentMod::Conn made;
    }

    constructor() {
        made = new LentMod::Conn();
    }
}

# code that calls the constructor: a Program that was restricted (only from loading native code, which is never lent,
# so the objects that it creates are still lent: a call of a lent object's method from it is a capability call)
Program caller(PO_MODERN | PO_NO_UNCONTROLLED_APIS);
caller.importClass("Holder");
caller.parse("object sub make() { return new Holder(); }", "caller");

# code restricted from the network that the objects are handed to
Program borrower(PO_MODERN | PO_NO_NETWORK);
borrower.parse("string sub get(object c) { return c.get(); }", "borrower");

Holder own();
printf("host: holder: %s\n", borrower.callFunction("get", own));
printf("host: made in its constructor: %s\n", borrower.callFunction("get", own.made));

object lent = caller.callFunction("make");
printf("caller: holder: %s\n", borrower.callFunction("get", lent));
printf("caller: made in its constructor: %s\n", borrower.callFunction("get", lent.made));
