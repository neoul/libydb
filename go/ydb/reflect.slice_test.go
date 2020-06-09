package ydb

import (
	"reflect"
	"testing"
)

func TestSliceSearch(t *testing.T) {
	type args struct {
		slice interface{}
		key   interface{}
	}
	tests := []struct {
		name  string
		args  args
		want  int
		want1 bool
	}{
		{
			name: "search value in a slice",
			args: args{
				slice: []int{10, 20, 50, 40, 30, 60},
				key:   30,
			},
			want:  4,
			want1: true,
		},
		{
			name: "search value in a slice with different element type",
			args: args{
				slice: []int{10, 20, 50, 40, 30, 60},
				key:   "30",
			},
			want:  4,
			want1: true,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, got1 := SliceSearch(tt.args.slice, tt.args.key)
			if got != tt.want {
				t.Errorf("SliceSearch() got = %v, want %v", got, tt.want)
			}
			if got1 != tt.want1 {
				t.Errorf("SliceSearch() got1 = %v, want %v", got1, tt.want1)
			}
		})
	}
}

func TestSliceDelete(t *testing.T) {
	type args struct {
		slice interface{}
		i     int
	}
	tests := []struct {
		name string
		args args
	}{
		{
			name: "delete an element value in the slice",
			args: args{
				slice: &([]int{10, 20, 50, 40, 30, 60}),
				i:     3,
			},
		},
		{
			name: "delete an element value in the slice with different element type",
			args: args{
				slice: &([]int{10, 20, 50, 40, 30, 60}),
				i:     0,
			},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			t.Log("Before", tt.args.slice)
			SliceDelete(tt.args.slice, tt.args.i)
			t.Log("After", tt.args.slice)
		})
	}
}

func TestSliceInsert(t *testing.T) {
	type args struct {
		slice interface{}
		i     int
		val   interface{}
	}
	tests := []struct {
		name string
		args args
	}{
		{
			name: "Insert an element value in the slice",
			args: args{
				slice: &([]int{10, 20, 50, 40, 30, 60}),
				i:     3,
				val:   70,
			},
		},
		{
			name: "Insert an element value in the slice with different element type",
			args: args{
				slice: &([]int{10, 20, 50, 40, 30, 60}),
				i:     0,
				val:   "70",
			},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			t.Log("Before", tt.args.slice)
			SliceInsert(tt.args.slice, tt.args.i, tt.args.val)
			t.Log("After", tt.args.slice)
		})
	}
}

func TestSliceDeleteCopy(t *testing.T) {
	type args struct {
		slice []int
		i     int
	}
	tests := []struct {
		name string
		args args
	}{
		{
			name: "delete an element value in the slice",
			args: args{
				slice: []int{10, 20, 50, 40, 30, 60},
				i:     3,
			},
		},
		{
			name: "delete an element value in the slice with different element type",
			args: args{
				slice: []int{10, 20, 50, 40, 30, 60},
				i:     0,
			},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := tt.args.slice
			SliceDeleteCopy(&tt.args.slice, tt.args.i)
			t.Log("After", s)
			t.Log("After", tt.args.slice)
		})
	}
}

func TestSliceInsertCopy(t *testing.T) {
	type args struct {
		slice []int
		i     int
		val   interface{}
	}
	tests := []struct {
		name string
		args args
	}{
		{
			name: "Insert an element value in the slice",
			args: args{
				slice: []int{10, 20, 50, 40, 30, 60},
				i:     3,
				val:   70,
			},
		},
		{
			name: "Insert an element value in the slice with different element type",
			args: args{
				slice: []int{10, 20, 50, 40, 30, 60},
				i:     0,
				val:   "70",
			},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			s := tt.args.slice
			SliceInsertCopy(&tt.args.slice, tt.args.i, tt.args.val)
			t.Log("After", s)
			t.Log("After", tt.args.slice)
		})
	}
}

func TestValSliceSearch(t *testing.T) {
	type args struct {
		v   reflect.Value
		key interface{}
	}
	tests := []struct {
		name  string
		args  args
		want  int
		want1 bool
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, got1 := ValSliceSearch(tt.args.v, tt.args.key)
			if got != tt.want {
				t.Errorf("ValSliceSearch() got = %v, want %v", got, tt.want)
			}
			if got1 != tt.want1 {
				t.Errorf("ValSliceSearch() got1 = %v, want %v", got1, tt.want1)
			}
		})
	}
}

func TestValSliceDelete(t *testing.T) {
	type args struct {
		v reflect.Value
		i int
	}
	tests := []struct {
		name    string
		args    args
		wantErr bool
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if err := ValSliceDelete(tt.args.v, tt.args.i); (err != nil) != tt.wantErr {
				t.Errorf("ValSliceDelete() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}

func TestValSliceInsert(t *testing.T) {
	type args struct {
		v   reflect.Value
		i   int
		val interface{}
	}
	tests := []struct {
		name    string
		args    args
		wantErr bool
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if err := ValSliceInsert(tt.args.v, tt.args.i, tt.args.val); (err != nil) != tt.wantErr {
				t.Errorf("ValSliceInsert() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}

func TestValSliceDeleteCopy(t *testing.T) {
	type args struct {
		v reflect.Value
		i int
	}
	tests := []struct {
		name    string
		args    args
		wantErr bool
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if err := ValSliceDeleteCopy(tt.args.v, tt.args.i); (err != nil) != tt.wantErr {
				t.Errorf("ValSliceDeleteCopy() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}

func TestValSliceInsertCopy(t *testing.T) {
	type args struct {
		v   reflect.Value
		i   int
		val interface{}
	}
	tests := []struct {
		name    string
		args    args
		wantErr bool
	}{
		// TODO: Add test cases.
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if err := ValSliceInsertCopy(tt.args.v, tt.args.i, tt.args.val); (err != nil) != tt.wantErr {
				t.Errorf("ValSliceInsertCopy() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}
