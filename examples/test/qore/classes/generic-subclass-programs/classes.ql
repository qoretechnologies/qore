class GenericBase<T> {
    public {
        *T value;
        *GenericSub<T> next;
    }
}
class GenericSub<T> inherits GenericBase<T> {}
class ConcreteSub inherits GenericSub<string> {}
