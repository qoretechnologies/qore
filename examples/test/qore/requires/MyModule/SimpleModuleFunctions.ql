# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SimpleModuleFunctions.ql consists functions definitions

public namespace EXAMPLE_F {
    public string sub func(string str) {
        return str + "EXAMPLE_F::func\n";
    }
}
