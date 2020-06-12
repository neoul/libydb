package ydb

import (
	"reflect"
	"testing"
)

type sfield struct {
	I int
	S string
}

type samplestruct struct {
	I      int
	P      *string
	Sfield sfield
	Smap   map[string]int
	Sslice []float32
}

func TestValNewStruct(t *testing.T) {
	type args struct {
		t reflect.Type
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
			wantEqual: false,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := ValStructNew(tt.args.t)
			if got.IsValid() {
				t.Log(got.Type(), got.Kind(), got)
			}
			if (err != nil) != tt.wantErr {
				t.Errorf("ValStructNew() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !reflect.DeepEqual(got, tt.want) && tt.wantEqual {
				t.Errorf("ValStructNew() = %v, want %v", got, tt.want)
			}
		})
	}
}
