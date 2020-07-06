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
			wantEqual: false,
		},
		{
			name: "init",
			args: args{
				t: reflect.TypeOf(samplestruct{}),
				// v: "[I=10][P=STR][Sfield=[I=20][S=hello]]",
				v: "[I=10][P=STR]",
			},
			want:      reflect.ValueOf(samplestruct{}),
			wantErr:   false,
			wantEqual: false,
		},
		{
			name: "init",
			args: args{
				t: reflect.TypeOf(samplestruct{}),
				v: "[I=10][P=STR][Sfield=[I=20][S=hello]]",
			},
			want:      reflect.ValueOf(samplestruct{}),
			wantErr:   false,
			wantEqual: false,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := ValStructNew(tt.args.t, tt.args.v, true)
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
			t.Log(DebugValueString(got.Interface(), 2, nil))
		})
	}
}

func TestDisassembleStructName(t *testing.T) {
	type args struct {
		keyVal interface{}
	}
	tests := []struct {
		name  string
		args  args
		want  interface{}
		want1 map[string]string
		want2 bool
	}{
		{
			name:  "1",
			args:  args{keyVal: "interface[name=1/1]"},
			want:  "interface",
			want1: map[string]string{"name": "1/1"},
			want2: true,
		},
		{
			name:  "2",
			args:  args{keyVal: "interface[name=]"},
			want:  "interface",
			want1: map[string]string{"name": ""},
			want2: true,
		},
		{
			name:  "3",
			args:  args{keyVal: "interface"},
			want:  "interface",
			want1: nil,
			want2: false,
		},
		{
			name:  "4",
			args:  args{keyVal: "[name=1/1]"},
			want:  "",
			want1: map[string]string{"name": "1/1"},
			want2: true,
		},
		{
			name:  "5",
			args:  args{keyVal: ""},
			want:  "",
			want1: nil,
			want2: false,
		},
		{
			name:  "6",
			args:  args{keyVal: "multikeylist[str=STR][integer=10]"},
			want:  "multikeylist",
			want1: map[string]string{"str": "STR", "integer": "10"},
			want2: true,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, got1, got2 := DisassembleStructString(tt.args.keyVal)
			if !reflect.DeepEqual(got, tt.want) {
				t.Errorf("DisassembleStructString() got = %v, want %v", got, tt.want)
			}
			if !reflect.DeepEqual(got1, tt.want1) {
				t.Errorf("DisassembleStructString() got1 = %v, want %v", got1, tt.want1)
			}
			if got2 != tt.want2 {
				t.Errorf("DisassembleStructString() got2 = %v, want %v", got2, tt.want2)
			}
		})
	}
}

func TestExtractStructName(t *testing.T) {
	type args struct {
		s interface{}
	}
	tests := []struct {
		name    string
		args    args
		want    string
		want1   map[string]string
		wantErr bool
	}{
		// TODO: Add test cases.
		{
			name:    "case",
			args:    args{s: "multikeylist[str=STR][integer=10]"},
			want:    "multikeylist",
			want1:   map[string]string{"str": "STR", "integer": "10"},
			wantErr: false,
		},
		{
			name:    "case",
			args:    args{s: "[str=STR][integer=10]"},
			want:    "",
			want1:   map[string]string{"str": "STR", "integer": "10"},
			wantErr: false,
		},
		{
			name:    "case",
			args:    args{s: "abc"},
			want:    "abc",
			want1:   nil,
			wantErr: false,
		},
		{
			name:    "case",
			args:    args{s: "abc[a="},
			want:    "abc[a=",
			want1:   nil,
			wantErr: true,
		},
		{
			name:    "case",
			args:    args{s: "abc[a=' ']"},
			want:    "abc",
			want1:   map[string]string{"a": " "},
			wantErr: false,
		},
		{
			name:    "case",
			args:    args{s: "abc]["},
			want:    "abc][",
			want1:   nil,
			wantErr: true,
		},
		{
			name:    "case",
			args:    args{s: "abc[a=']']"},
			want:    "abc",
			want1:   map[string]string{"a": "]"},
			wantErr: false,
		},
		{
			name:    "case",
			args:    args{s: "AAA[I=10][P=STR][Sfield=[I=20][S=hello]]"},
			want:    "AAA",
			want1:   map[string]string{"I": "10", "P": "STR", "Sfield": "[I=20][S=hello]"},
			wantErr: false,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, got1, err := ExtractStructName(tt.args.s)
			if (err != nil) != tt.wantErr {
				t.Errorf("ExtractStructName() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if got != tt.want {
				t.Errorf("ExtractStructName() got = %v, want %v", got, tt.want)
			}
			if !reflect.DeepEqual(got1, tt.want1) {
				t.Errorf("ExtractStructName() got1 = %v, want %v", got1, tt.want1)
			}
		})
	}
}
