package ydb

import (
	"reflect"
	"testing"
)

func TestTypeGetAll(t *testing.T) {
	type testX struct {
		a int
		B string
		C *int
	}
	type testY struct {
		a map[string]testX
		A map[string]testX
		b []testX
		C []interface{}
	}
	type args struct {
		t reflect.Type
	}
	tests := []struct {
		name  string
		args  args
		want  []reflect.Type
		want1 bool
	}{
		{
			name: "TypeGetAll",
			args: args{
				t: reflect.TypeOf(nil),
			},
			want:  []reflect.Type{},
			want1: false,
		},
		{
			name: "TypeGetAll",
			args: args{
				t: reflect.TypeOf(testY{}),
			},
			want:  []reflect.Type{reflect.TypeOf(map[string]testX{}), reflect.TypeOf([]interface{}{})},
			want1: true,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, got1 := TypeGetAll(tt.args.t)
			if !reflect.DeepEqual(got, tt.want) {
				t.Errorf("TypeGetAll() got = %v, want %v", got, tt.want)
			}
			if got1 != tt.want1 {
				t.Errorf("TypeGetAll() got1 = %v, want %v", got1, tt.want1)
			}
		})
	}
}
