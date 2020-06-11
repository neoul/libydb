package ydb

import (
	"reflect"
	"testing"
)

func TestValNewScalar(t *testing.T) {
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
			got, err := ValNewScalar(tt.args.t, tt.args.val)
			t.Log(got, got.Type(), got.Kind())
			t.Log(tt.want, tt.want.Type(), tt.want.Kind())
			if (err != nil) != tt.wantErr {
				t.Errorf("ValNewScalar() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			cgot := got
			ttw := tt.want
			for {
				if ttw.Kind() != reflect.Ptr || cgot.Kind() != reflect.Ptr {
					if ttw.Kind() != cgot.Kind() {
						t.Errorf("ValNewScalar() has different type (%s, %s)", ttw.Kind(), cgot.Kind())
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
				t.Errorf("ValNewScalar() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestValSetScalar(t *testing.T) {
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
			err := ValSetScalar(tt.args.v, tt.args.val)
			v := tt.args.v
			for v.IsValid() {
				t.Log(v.Type(), v.Kind(), v)
				if v.Kind() != reflect.Ptr {
					break
				}
				v = v.Elem()
			}
			if (err != nil) != tt.wantErr {
				t.Errorf("ValSetScalar() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
	t.Logf("v0 %s", v0)
	t.Logf("v1 %s", *v1)
	t.Logf("v2 %s", **v2)
	t.Logf("v3 %s", *v3)
}
