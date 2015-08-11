// Autogenerated by metajava.py.
// Do not edit this file directly.
// The template for this file is located at:
// ../../../../../../../../templates/AstSubclass.java
package com.rethinkdb.ast.gen;

import com.rethinkdb.model.Arguments;
import com.rethinkdb.model.OptArgs;
import com.rethinkdb.ast.ReqlAst;
import com.rethinkdb.proto.TermType;


public class Sync extends ReqlQuery {


    public Sync(java.lang.Object arg) {
        this(new Arguments(arg), null);
    }
    public Sync(Arguments args, OptArgs optargs) {
        this(null, args, optargs);
    }
    public Sync(ReqlAst prev, Arguments args, OptArgs optargs) {
        this(prev, TermType.SYNC, args, optargs);
    }
    protected Sync(ReqlAst previous, TermType termType, Arguments args, OptArgs optargs){
        super(previous, termType, args, optargs);
    }


    /* Static factories */
    public static Sync fromArgs(java.lang.Object... args){
        return new Sync(new Arguments(args), null);
    }


}