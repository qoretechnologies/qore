%new-style


class A {
   constructor() {
     printf("A::constructor\n");
   }
   destructor() {
     printf("A::destructor\n");
   }
}

class B inherits A {
   constructor() {
     printf("B::constructor\n");
   }
   destructor() {
     printf("B::destructor\n");
   }
}
class C inherits B{
   constructor() {
     printf("C::constructor\n");
   }
   destructor() {
     printf("C::destructor\n");
   }
}


A a = new A();

delete a;
a = new B();
a = new C();
delete a;

class P inherits Program {
    constructor(): Program(PO_DEFAULT) {
        printf("P::constructor\n");
    }
    destructor() {
        printf("P::destructor\n");

    }
}

Program p = new P();
delete p;

p = new Program();
delete p;

class P2 inherits Program {
    constructor(): Program(PO_DEFAULT) {
        printf("P2::constructor\n");
    }
}

class P3 inherits P2 {
    constructor(): P2() {
        printf("P3::constructor\n");
    }
}

p = new P2();
delete p;

p = new P3();
delete p;

P2 p3();

delete p3;

Program pp(PO_DEFAULT);
delete pp;
