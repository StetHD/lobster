// Copyright 2014 Wouter van Oortmerssen. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stdafx.h"

#include "vmdata.h"

namespace lobster {

BoxedInt::BoxedInt(int _v) : RefObj(g_vm->GetTypeInfo(TYPE_ELEM_BOXEDINT)), val(_v) {}
BoxedFloat::BoxedFloat(float _v) : RefObj(g_vm->GetTypeInfo(TYPE_ELEM_BOXEDFLOAT)), val(_v) {}
LString::LString(int _l) : RefObj(g_vm->GetTypeInfo(TYPE_ELEM_STRING)), len(_l) {}

char HexChar(char i) { return i + (i < 10 ? '0' : 'A' - 10); }

void EscapeAndQuote(const string &s, string &r) {
    r += "\"";
    for (size_t i = 0; i < s.length(); i++) switch(s[i]) {
        case '\n': r += "\\n"; break;
        case '\t': r += "\\t"; break;
        case '\r': r += "\\r"; break;
        case '\\': r += "\\\\"; break;
        case '\"': r += "\\\""; break;
        case '\'': r += "\\\'"; break;
        default:
            if (s[i] >= ' ' && s[i] <= '~') r += s[i];
            else {
                r += "\\x"; r += HexChar(((uchar)s[i]) >> 4); r += HexChar(s[i] & 0xF); }
            break;
    }
    r += "\"";
}

string LString::ToString(PrintPrefs &pp) {
    if (pp.cycles >= 0) {
        if (refc < 0)
            return CycleStr();
        CycleDone(pp.cycles);
    }
    string s = len > pp.budget ? string(str()).substr(0, pp.budget) + ".." : str();
    if (pp.quoted) {
        string r;
        EscapeAndQuote(s, r);
        return r;
    } else {
        return s;
    }
}

int ElemObj::Len() const {
    if (ti.t == V_VECTOR) return ((LVector *)this)->len;
    assert(ti.t == V_STRUCT);
    return ti.len;
}

Value &ElemObj::At(int i) const {
    if (ti.t == V_VECTOR) return ((LVector *)this)->At(i);
    assert(ti.t == V_STRUCT);
    return ((LStruct *)this)->At(i);
};

ValueType ElemObj::ElemType(int i) const {
    auto &sti = g_vm->GetTypeInfo(ti.t == V_VECTOR ? ti.subt : ti.elems[i]);
    auto vt = sti.t;
    if (vt == V_NIL) vt = g_vm->GetTypeInfo(sti.subt).t;
    #if RTT_ENABLED
    // FIXME: for testing
    if(vt != At(i).type && At(i).type != V_NIL && !(vt == V_VECTOR && At(i).type == V_STRUCT)) {
        Output(OUTPUT_INFO, "elemtype of %s != %s", ti.Debug().c_str(), BaseTypeName(At(i).type));
        assert(false);
    }
    #endif
    return vt;
}

string ElemObj::ToString(PrintPrefs &pp) {
    if (pp.cycles >= 0) {
        if (refc < 0)
            return CycleStr();
        CycleDone(pp.cycles);
    }
    string s = ti.t == V_STRUCT ? g_vm->ReverseLookupType(ti.structidx) + string("{") : "[";
    for (int i = 0; i < Len(); i++) {
        if (i) s += ", ";
        if ((int)s.size() > pp.budget) { s += "...."; break; }
        PrintPrefs subpp(pp.depth - 1, pp.budget - (int)s.size(), true, pp.decimals, pp.anymark);
        s += pp.depth || !IsRef(ElemType(i)) ? At(i).ToString(ElemType(i), subpp) : "..";
    }
    s += ti.t == V_STRUCT ? "}" : "]";
    return s;
}

LVector::LVector(int _initial, int _max, const TypeInfo &_ti)
    : ElemObj(_ti), len(_initial), maxl(_max) {
    v = maxl ? AllocSubBuf<Value>(maxl, g_vm->GetTypeInfo(TYPE_ELEM_VALUEBUF)) : nullptr;
}

const TypeInfo &LVector::ElemTypeInfo() const { return g_vm->GetTypeInfo(ti.subt); }

void LVector::Resize(int newmax) {
    // FIXME: check overflow
    auto mem = AllocSubBuf<Value>(newmax, g_vm->GetTypeInfo(TYPE_ELEM_VALUEBUF));
    if (len) memcpy(mem, v, sizeof(Value) * len);
    DeallocBuf();
    maxl = newmax;
    v = mem;
}

void LVector::Append(LVector *from, int start, int amount) {
    if (len + amount > maxl) Resize(len + amount);  // FIXME: check overflow
    memcpy(v + len, from->v + start, sizeof(Value) * amount);
    if (IsRefNil(from->ElemTypeInfo().t)) {
        for (int i = 0; i < amount; i++) v[len + i].INCRTNIL();
    }
    len += amount;
}

void RefObj::DECDELETE(bool deref) {
    assert(refc == 0);
    switch (ti.t) {
        case V_BOXEDINT:   vmpool->dealloc(this, sizeof(BoxedInt)); break;
        case V_BOXEDFLOAT: vmpool->dealloc(this, sizeof(BoxedFloat)); break;
        case V_STRING:     ((LString *)this)->DeleteSelf(); break;
        case V_COROUTINE:  ((CoRoutine *)this)->DeleteSelf(deref); break;
        case V_VECTOR:     ((LVector *)this)->DeleteSelf(deref); break;
        case V_STRUCT:     ((LStruct *)this)->DeleteSelf(deref); break;
        default:           assert(false);
    }
}

bool RefEqual(const RefObj *a, const RefObj *b, bool structural) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (&a->ti != &b->ti) return false;
    switch (a->ti.t) {
        case V_BOXEDINT:    return ((BoxedInt *)a)->val == ((BoxedInt *)b)->val;
        case V_BOXEDFLOAT:  return ((BoxedFloat *)a)->val == ((BoxedFloat *)b)->val;
        case V_STRING:      return *((LString *)a) == *((LString *)b);
        case V_COROUTINE:   return false;
        case V_VECTOR:
        case V_STRUCT:      return structural && ((ElemObj *)a)->Equal(*(ElemObj *)b);
        default:            assert(0); return false;
    }
}

bool Value::Equal(ValueType vtype, const Value &o, ValueType otype, bool structural) const {
    if (vtype != otype) return false;
    switch (vtype) {
        case V_INT: return ival_ == o.ival_;
        case V_FLOAT: return fval_ == o.fval_;
        case V_FUNCTION: return ip_ == o.ip_;
        default: return RefEqual(refnil(), o.ref_, structural);
    }
}

string RefToString(const RefObj *ro, PrintPrefs &pp) {
    if (!ro) return "nil";
    switch (ro->ti.t) {
        case V_BOXEDINT: {
            auto s = to_string(((BoxedInt *)ro)->val);
            return pp.anymark ? "#" + s : s;
        }
        case V_BOXEDFLOAT: {
            auto s = to_string_float(((BoxedFloat *)ro)->val, pp.decimals);
            return pp.anymark ? "#" + s : s;
        }
        case V_STRING:     return ((LString *)ro)->ToString(pp);
        case V_COROUTINE:  return "(coroutine)";
        case V_VECTOR:
        case V_STRUCT:     return ((ElemObj *)ro)->ToString(pp);
        default:           return string("(") + BaseTypeName(ro->ti.t) + ")";
    }
}

string Value::ToString(ValueType vtype, PrintPrefs &pp) const {
    if (IsRefNil(vtype)) return ref_ ? RefToString(ref_, pp) : "nil";
    switch (vtype) {
        case V_INT:        return to_string(ival());
        case V_FLOAT:      return to_string_float(fval(), pp.decimals);
        case V_FUNCTION:   return "<FUNCTION>";
        default:           return string("(") + BaseTypeName(vtype) + ")";
    }
}


void RefObj::Mark() {
    if (refc < 0) return;
    assert(refc);
    refc = -refc;
    switch (ti.t) {
        case V_STRUCT:
        case V_VECTOR:     ((ElemObj   *)this)->Mark(); break;
        case V_COROUTINE:  ((CoRoutine *)this)->Mark(); break;
    }
}

void Value::Mark(ValueType vtype) {
    if (IsRefNil(vtype) && ref_) ref_->Mark();
}

void Value::MarkRef() {
    if (ref_) ref_->Mark();
}

string TypeInfo::Debug(bool rec) const {
    string s = BaseTypeName(t);
    if (t == V_VECTOR || t == V_NIL) {
        s += "[" + g_vm->GetTypeInfo(subt).Debug(false) + "]";
    } else if (t == V_STRUCT) {
        auto sname = g_vm->StructName(*this);
        s += ":" + sname;
        if (rec) {
            s += "{";
            for (int i = 0; i < len; i++)
                s += g_vm->GetTypeInfo(elems[i]).Debug(false) + ",";
            s += "}";
        }
    }
    return s;
}

}  // namespace lobster
