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

func TestExtractStructNameAndValue(t *testing.T) {
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
			got, got1, err := ExtractStrValNameAndValue(tt.args.s)
			if (err != nil) != tt.wantErr {
				t.Errorf("ExtractStrValNameAndValue() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if got != tt.want {
				t.Errorf("ExtractStrValNameAndValue() got = %v, want %v", got, tt.want)
			}
			if !reflect.DeepEqual(got1, tt.want1) {
				t.Errorf("ExtractStrValNameAndValue() got1 = %v, want %v", got1, tt.want1)
			}
		})
	}
}
