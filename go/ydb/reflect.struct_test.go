package ydb

import (
	"reflect"
	"testing"
)

func TestValNewStruct(t *testing.T) {
	p := "STR"
	type args struct {
		t reflect.Type
		v string
	}
	tests := []struct {
		name      string
		args      args
		want      reflect.Value
		wantErr   bool
		wantEqual bool
	}{
		{
			name: "init",
			args: args{
				t: reflect.TypeOf(samplestruct{}),
			},
			want:      reflect.ValueOf(samplestruct{}),
			wantErr:   false,
			wantEqual: true,
		},
		{
			name: "init",
			args: args{
				t: reflect.TypeOf(samplestruct{}),
				v: "[I=10][P=STR]",
			},
			want:      reflect.ValueOf(samplestruct{I: 10, P: &p}),
			wantErr:   false,
			wantEqual: true,
		},
		{
			name: "init",
			args: args{
				t: reflect.TypeOf(samplestruct{}),
				v: "[I=10][P=STR][Sfield=[I=20][S=hello]]", // Support hierarchical structure
			},
			want:      reflect.ValueOf(samplestruct{I: 10, P: &p, Sfield: sfield{I: 20, S: "hello"}}),
			wantErr:   false,
			wantEqual: true,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := ValStructNew(tt.args.t, tt.args.v, false)
			// if got.IsValid() {
			// 	t.Log(got.Type(), got.Kind(), got)
			// }
			if (err != nil) != tt.wantErr {
				t.Errorf("ValStructNew() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !reflect.DeepEqual(got.Interface(), tt.want.Interface()) && tt.wantEqual {
				t.Errorf("ValStructNew() = %v, want %v", got, tt.want)
			}
			t.Log(DebugValueString(got.Interface(), 2, nil))
		})
	}
}
