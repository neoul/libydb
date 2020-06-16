package ydb

import (
	"reflect"
	"testing"
)

func TestValYdbSetAndUnset(t *testing.T) {
	m := reflect.ValueOf(map[string]interface{}{})
	type args struct {
		v     reflect.Value
		keys  []string
		key   string
		tag   string
		value string
	}
	testSet := []struct {
		name    string
		args    args
		wantErr bool
	}{
		{
			name: "Create !!seq",
			args: args{
				v:     m,
				keys:  []string{},
				key:   "list",
				tag:   "!!seq",
				value: "",
			},
			wantErr: false,
		},
		{
			name: "Create !!map",
			args: args{
				v:     m,
				keys:  []string{"list"},
				key:   "",
				tag:   "!!map",
				value: "",
			},
			wantErr: false,
		},
		{
			name: "Create !!str",
			args: args{
				v:     m,
				keys:  []string{"list", ""},
				key:   "e-mail",
				tag:   "!!string",
				value: "neoul@ymail.com",
			},
			wantErr: false,
		},
		{
			name: "Create !!str",
			args: args{
				v:     m,
				keys:  []string{"list", ""},
				key:   "name",
				tag:   "!!string",
				value: "neoul",
			},
			wantErr: false,
		},
		{
			name: "Create another !!map in !!seq",
			args: args{
				v:     m,
				keys:  []string{"list"},
				key:   "",
				tag:   "!!map",
				value: "",
			},
			wantErr: false,
		},
		{
			name: "Create !!str",
			args: args{
				v:     m,
				keys:  []string{"list", ""},
				key:   "e-mail",
				tag:   "!!string",
				value: "example@google.com",
			},
			wantErr: false,
		},
		{
			name: "Create !!str",
			args: args{
				v:     m,
				keys:  []string{"list", ""},
				key:   "name",
				tag:   "!!string",
				value: "example",
			},
			wantErr: false,
		},
	}
	for _, tt := range testSet {
		t.Run(tt.name, func(t *testing.T) {
			t.Logf("%v, %v, %v, %v", tt.args.keys, tt.args.key, tt.args.tag, tt.args.value)
			if err := ValYdbSet(tt.args.v, tt.args.keys, tt.args.key, tt.args.tag, tt.args.value); (err != nil) != tt.wantErr {
				t.Errorf("ValYdbSet() error = %v, wantErr %v", err, tt.wantErr)
			}
			t.Log(tt.args.v)
		})
	}

	testUnset := []struct {
		name    string
		args    args
		wantErr bool
	}{
		{
			name: "Delete !!str",
			args: args{
				v:     m,
				keys:  []string{"list", ""},
				key:   "name",
				tag:   "!!string",
				value: "example",
			},
			wantErr: false,
		},
		{
			name: "Delete !!str",
			args: args{
				v:     m,
				keys:  []string{"list", ""},
				key:   "e-mail",
				tag:   "!!string",
				value: "example@google.com",
			},
			wantErr: false,
		},
		{
			name: "Delete another !!map in !!seq",
			args: args{
				v:     m,
				keys:  []string{"list"},
				key:   "",
				tag:   "!!map",
				value: "",
			},
			wantErr: false,
		},
		{
			name: "Delete !!str",
			args: args{
				v:     m,
				keys:  []string{"list", ""},
				key:   "name",
				tag:   "!!string",
				value: "neoul",
			},
			wantErr: false,
		},
		{
			name: "Delete !!str",
			args: args{
				v:     m,
				keys:  []string{"list", ""},
				key:   "e-mail",
				tag:   "!!string",
				value: "neoul@ymail.com",
			},
			wantErr: false,
		},
		{
			name: "Delete !!map",
			args: args{
				v:     m,
				keys:  []string{"list"},
				key:   "",
				tag:   "!!map",
				value: "",
			},
			wantErr: false,
		},
		{
			name: "Delete !!seq",
			args: args{
				v:     m,
				keys:  []string{},
				key:   "list",
				tag:   "!!seq",
				value: "",
			},
			wantErr: false,
		},
	}
	for _, tt := range testUnset {
		t.Run(tt.name, func(t *testing.T) {
			t.Logf("%v, %v", tt.args.keys, tt.args.key)
			if err := ValYdbUnset(tt.args.v, tt.args.keys, tt.args.key); (err != nil) != tt.wantErr {
				t.Errorf("ValYdbUnset() error = %v, wantErr %v", err, tt.wantErr)
			}
			t.Log(tt.args.v)
		})
	}
}
