package ydb

import (
	"reflect"
	"testing"

	"gopkg.in/yaml.v2"
)

func TestYDB_ReadWrite(t *testing.T) {
	db, dbclose := Open("TestYDB_ReadWrite")
	defer dbclose()
	type args struct {
		writeYaml    string
		deleteYaml   string
		readYaml     string
		writeToPath  string
		writeToValue string
		deleteFrom   string
		readFrom     string
	}
	tests := []struct {
		name         string
		db           *YDB
		args         args
		wantWriteErr bool
		readString   string
	}{
		{
			name: "write",
			args: args{
				writeYaml: `
hello:
  ydb: 월드
good:
  ydb: bye
list:
  - entry 1
  - entry 2
`,
			},
			wantWriteErr: false,
		},
		{
			name: "write-error(tab)",
			args: args{
				writeYaml: `
hello:
  ydb: world
good:
  ydb: bye
list:
  - entry 1
  - entry 2
		`,
			},
			wantWriteErr: true,
		},
		{
			name: "read",
			args: args{
				readYaml: `
hello:
  ydb:
good:
  ydb:
`,
			},
			readString: `
hello:
  ydb: 월드
good:
  ydb: bye
`,
		},
		{
			name: "readFrom",
			args: args{
				readFrom: "/hello/ydb",
			},
			readString: `월드`,
		},
		{
			name: "replace",
			args: args{
				writeYaml: `
hello:
  ydb: yaml data block
good:
  ydb: morning
list:
  - entry 한글
`,
			},
			wantWriteErr: false,
		},
		{
			name: "read",
			args: args{
				readYaml: `
hello:
  ydb:
good:
  ydb:
list:
`,
			},
			readString: `
hello:
  ydb: yaml data block
good:
  ydb: morning
list:
  - entry 1
  - entry 2
  - entry 한글
`,
		},
		{
			name: "delete",
			args: args{
				deleteYaml: `
hello:
  ydb:
good:
  ydb: morning
list:
  - ..
`,
			},
			wantWriteErr: false,
		},
		{
			name: "read",
			args: args{
				readYaml: `
hello:
  ydb:
good:
  ydb:
list:
`,
			},
			readString: `
list:
  - entry 2
  - entry 한글
`,
		},
		{
			name: "writeTo",
			args: args{
				writeToPath:  "/hello/ydb",
				writeToValue: "world",
			},
			wantWriteErr: false,
		},
		{
			name: "readFrom",
			args: args{
				readFrom: "/hello/ydb",
			},
			readString: `world`,
		},
		{
			name: "readFrom",
			args: args{
				readFrom: "/good/ydb",
			},
			readString: ``,
		},
		{
			name: "readFrom",
			args: args{
				readFrom: "/good",
			},
			readString: ``,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if tt.args.writeYaml != "" {
				if err := db.Write([]byte(tt.args.writeYaml)); (err != nil) != tt.wantWriteErr {
					t.Errorf("YDB.Write() error = %v, wantWriteErr %v", err, tt.wantWriteErr)
				}
			}
			if tt.args.writeToPath != "" {
				if err := db.WriteTo(tt.args.writeToPath, tt.args.writeToValue); (err != nil) != tt.wantWriteErr {
					t.Errorf("YDB.Delete() error = %v, wantWriteErr %v", err, tt.wantWriteErr)
				}
			}
			if tt.args.deleteYaml != "" {
				if err := db.Delete([]byte(tt.args.deleteYaml)); (err != nil) != tt.wantWriteErr {
					t.Errorf("YDB.Delete() error = %v, wantWriteErr %v", err, tt.wantWriteErr)
				}
			}
			if tt.args.deleteFrom != "" {
				if err := db.DeleteFrom(tt.args.deleteFrom); (err != nil) != tt.wantWriteErr {
					t.Errorf("YDB.Delete() error = %v, wantWriteErr %v", err, tt.wantWriteErr)
				}
			}
			if tt.args.readYaml != "" {
				gotResult := map[string]interface{}{}
				resResult := map[string]interface{}{}
				got := db.Read([]byte(tt.args.readYaml))
				err := yaml.Unmarshal(got, gotResult)
				// t.Logf("got=\n%s", string(got))
				if err != nil {
					t.Errorf("YDB.Unmarshal(got) error = %v", err)
				}
				err = yaml.Unmarshal([]byte(tt.readString), resResult)
				if err != nil {
					t.Errorf("YDB.Unmarshal(readString) error = %v", err)
				}
				// t.Logf("readString=\n%s", tt.readString)
				if !reflect.DeepEqual(gotResult, resResult) {
					t.Errorf("got  = %v", string(got))
					t.Errorf("got  = %v", gotResult)
					t.Errorf("want = %v", resResult)
				}
			}
			if tt.args.readFrom != "" {
				got := db.ReadFrom(tt.args.readFrom)
				if !reflect.DeepEqual(got, tt.readString) {
					t.Errorf("got  = %v", got)
					t.Errorf("want  = %v", tt.readString)
				}
			}
		})
	}
}