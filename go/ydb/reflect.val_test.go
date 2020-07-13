package ydb

import (
	"reflect"
	"testing"
)

func TestValScalarNew(t *testing.T) {
	var s0 string = "10"
	var s1 *string = &s0

	type args struct {
		t   reflect.Type
		val interface{}
	}
	tests := []struct {
		name     string
		args     args
		want     reflect.Value
		wantErr  bool
		wantSame bool
	}{
		{
			name: "Create new string",
			args: args{
				t:   reflect.TypeOf(s0),
				val: 20,
			},
			want:     reflect.ValueOf("20"),
			wantErr:  false,
			wantSame: true,
		},
		{
			name: "Create new *string",
			args: args{
				t:   reflect.TypeOf(s1),
				val: 30,
			},
			want:     reflect.ValueOf(s1),
			wantErr:  false,
			wantSame: false,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := ValScalarNew(tt.args.t, tt.args.val)
			t.Log(got, got.Type(), got.Kind())
			t.Log(tt.want, tt.want.Type(), tt.want.Kind())
			if (err != nil) != tt.wantErr {
				t.Errorf("ValScalarNew() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			cgot := got
			ttw := tt.want
			for {
				if ttw.Kind() != reflect.Ptr || cgot.Kind() != reflect.Ptr {
					if ttw.Kind() != cgot.Kind() {
						t.Errorf("ValScalarNew() has different type (%s, %s)", ttw.Kind(), cgot.Kind())
						return
					}
					break
				}
				ttw = ttw.Elem()
				cgot = cgot.Elem()
				t.Log(cgot, cgot.Type(), cgot.Kind())
				t.Log(ttw, ttw.Type(), ttw.Kind())
			}
			if !reflect.DeepEqual(cgot.Interface(), ttw.Interface()) == tt.wantSame {
				t.Errorf("ValScalarNew() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestValScalarSet(t *testing.T) {
	var v0 string = "0"
	var v1 *string = &v0
	var v2 **string = &v1
	var v3 *string = nil
	type args struct {
		v   reflect.Value
		val interface{}
	}
	tests := []struct {
		name    string
		args    args
		wantErr bool
	}{
		{
			name: "SetScalar - not settable",
			args: args{
				v:   reflect.ValueOf(v0),
				val: 10,
			},
			wantErr: true,
		},
		{
			name: "SetScalar - non-ptr",
			args: args{
				v:   reflect.ValueOf(&v0),
				val: 10,
			},
			wantErr: false,
		},
		{
			name: "SetScalar - ptr",
			args: args{
				v:   reflect.ValueOf(v1),
				val: 20,
			},
			wantErr: false,
		},
		{
			name: "SetScalar - double ptr",
			args: args{
				v:   reflect.ValueOf(v2),
				val: 30,
			},
			wantErr: false,
		},
		{
			name: "SetScalar - nil ptr",
			args: args{
				v:   reflect.ValueOf(&v3),
				val: "40",
			},
			wantErr: false,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := ValScalarSet(tt.args.v, tt.args.val)
			v := tt.args.v
			for v.IsValid() {
				t.Log(v.Type(), v.Kind(), v)
				if v.Kind() != reflect.Ptr {
					break
				}
				v = v.Elem()
			}
			if (err != nil) != tt.wantErr {
				t.Errorf("ValScalarSet() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
	t.Logf("v0 %s", v0)
	t.Logf("v1 %s", *v1)
	t.Logf("v2 %s", **v2)
	t.Logf("v3 %s", *v3)
}

func TestValFind(t *testing.T) {
	nv, _ := ValStructNew(reflect.TypeOf(samplestruct{}), true)
	ss := nv.Interface().(samplestruct)
	ss.Smap["10"] = 10
	ss.Smap["20"] = 20
	ss.Smap["30"] = 30
	ss.I = 1
	ss.Sfield.S = "ss.s"
	ss.Sfield.I = 1000
	ss.Sslice = append(ss.Sslice, 0.1)
	ss.Sslice = append(ss.Sslice, 0.2)
	ss.Sslice = append(ss.Sslice, 0.3)

	type args struct {
		v    reflect.Value
		keys []string
	}
	tests := []struct {
		name  string
		args  args
		want  reflect.Value
		want1 bool
	}{
		{
			name: "find I",
			args: args{
				v:    reflect.ValueOf(ss),
				keys: []string{"I"},
			},
			want:  reflect.ValueOf(1),
			want1: true,
		},
		{
			name: "find Sfield",
			args: args{
				v:    reflect.ValueOf(ss),
				keys: []string{"Sfield", "S"},
			},
			want:  reflect.ValueOf(ss.Sfield.S),
			want1: true,
		},
		{
			name: "find Smap",
			args: args{
				v:    reflect.ValueOf(ss),
				keys: []string{"Smap", "10"},
			},
			want:  reflect.ValueOf(10),
			want1: true,
		},
		{
			name: "find Sslice",
			args: args{
				v:    reflect.ValueOf(ss),
				keys: []string{"Sslice", "1"},
			},
			want:  reflect.ValueOf(ss.Sslice[1]),
			want1: true,
		},
	}
	t.Log(ss)
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got1 := false
			got := tt.args.v
			for _, key := range tt.args.keys {
				got, got1 = ValFind(got, key, SearchByIndex)
				if got.IsValid() {
					t.Log(got.Type(), got.Kind(), got)
				}
				if !got1 {
					t.Errorf("ValFind() failed for '%s' searching", key)
					break
				}
			}
			if got.IsValid() {
				t.Log(tt.want.Type(), tt.want.Kind(), tt.want)
				t.Log(got.Type(), got.Kind(), got)
				if !reflect.DeepEqual(got.Interface(), tt.want.Interface()) {
					t.Errorf("ValFind() got = %v, want %v", got, tt.want)
				}
				if got1 != tt.want1 {
					t.Errorf("ValFind() got1 = %v, want %v", got1, tt.want1)
				}
			}
		})
	}
}

func _equal(d1, d2 []interface{}) bool {
	for len(d1) != len(d2) {
		return false
	}
	for _, x := range d1 {
		eq := false
		for _, y := range d2 {
			if reflect.DeepEqual(x, y) {
				eq = true
				break
			}
		}
		if !eq {
			return false
		}
	}
	return true
}

func TestValGetAll(t *testing.T) {
	type testX struct {
		a int
		B string
		C *int
	}
	type testY struct {
		A map[string]testX
		B map[string]testX
		C []testX
		D []interface{}
	}
	c := 20
	tX := testX{
		a: 10,
		B: "B",
		C: &c,
	}
	tY := &testY{
		A: map[string]testX{"tX1": tX, "tX2": tX},
		B: map[string]testX{"tX1": tX, "tX2": tX},
		C: []testX{tX, tX},
		D: []interface{}{tX},
	}
	type args struct {
		v reflect.Value
	}
	tests := []struct {
		name  string
		args  args
		want  []reflect.Value
		want1 bool
	}{
		{
			name: "ValGetAll",
			args: args{
				v: reflect.ValueOf(tY),
			},
			want: []reflect.Value{
				reflect.ValueOf(tY.A),
				reflect.ValueOf(tY.B),
				reflect.ValueOf(tY.C),
				reflect.ValueOf(tY.D)},
			want1: true,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, got1 := ValGetAll(tt.args.v)
			w1 := []interface{}{}
			for _, v := range tt.want {
				w1 = append(w1, v.Interface())
			}
			g1 := []interface{}{}
			for _, v := range got {
				if v.CanInterface() {
					g1 = append(g1, v.Interface())
				}
			}
			if !_equal(g1, w1) {
				t.Errorf("ValGetAll()")
				t.Errorf(" got = %v", g1)
				t.Errorf(" want %v", w1)
			}
			if got1 != tt.want1 {
				t.Errorf("ValGetAll() got1 = %v, want %v", got1, tt.want1)
			}
		})
	}
}
