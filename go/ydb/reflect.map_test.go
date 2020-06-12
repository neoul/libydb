package ydb

import (
	"reflect"
	"testing"
)

func TestValMapSet(t *testing.T) {
	m := map[interface{}]interface{}{}
	m[10] = 10
	m["11"] = "11"
	m["hello"] = "world"
	m["good"] = "bye"
	type args struct {
		v       reflect.Value
		key     interface{}
		element interface{}
	}
	tests := []struct {
		name    string
		args    args
		wantErr bool
	}{
		{
			name: "add an element 1",
			args: args{
				v:       reflect.ValueOf(m),
				key:     10,
				element: 20,
			},
			wantErr: false,
		},
		{
			name: "add an element 2",
			args: args{
				v:       reflect.ValueOf(m),
				key:     11, // int type 11 doesn't update string type "11".
				element: 22,
			},
			wantErr: false,
		},
		{
			name: "add an element 3",
			args: args{
				v:       reflect.ValueOf(m),
				key:     "11", // string type "11" is updated to 22.
				element: 22,
			},
			wantErr: false,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if err := ValMapSet(tt.args.v, tt.args.key, tt.args.element); (err != nil) != tt.wantErr {
				t.Errorf("ValMapSet() error = %v, wantErr %v", err, tt.wantErr)
			} else {
				t.Log(tt.args.v)
			}
		})
	}
}

func TestValMapUnset(t *testing.T) {
	m := map[interface{}]interface{}{}
	m[10] = 10
	m["11"] = "11"
	m["hello"] = "world"
	m["good"] = "bye"
	type args struct {
		v   reflect.Value
		key interface{}
	}
	tests := []struct {
		name    string
		args    args
		wantErr bool
	}{
		{
			name: "delete an element 1",
			args: args{
				v:   reflect.ValueOf(m),
				key: 10,
			},
			wantErr: false,
		},
		{
			name: "delete an element 2",
			args: args{
				v:   reflect.ValueOf(m),
				key: "11",
			},
			wantErr: false,
		},
	}
	t.Log(m)
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if err := ValMapUnset(tt.args.v, tt.args.key); (err != nil) != tt.wantErr {
				t.Errorf("ValMapUnset() error = %v, wantErr %v", err, tt.wantErr)
			} else {
				t.Log(tt.args.v)
			}
		})
	}
}
