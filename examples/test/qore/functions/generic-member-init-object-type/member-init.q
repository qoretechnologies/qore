%modern
class G<T> {
    string tn() { return (new list<T>()).fullType(); }
}
class H1<A> {
    public { G<float> gf = new G<float>(); }
    string tn() { return gf.tn(); }
}
class H2<A> {
    public { G<float> gf(); }
    string tn() { return gf.tn(); }
}
class H3 {
    public { G<float> gf(); }
    string tn() { return gf.tn(); }
}
class H4<A> {
    private { G<A> ga(); }
    string tn() { return ga.tn(); }
}
G<float> local();
printf("local %s\n", local.tn());
H1<int> h1();
printf("h1 %s\n", h1.tn());
H3 h3();
printf("h3 %s\n", h3.tn());
H4<int> h4();
printf("h4 %s\n", h4.tn());
H2<int> h2();
printf("h2 %s\n", h2.tn());
