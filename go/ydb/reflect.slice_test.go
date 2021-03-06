package ydb

import (
	"fmt"
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
			got, got1 := SliceFind(tt.args.slice, tt.args.key)
			if got != tt.want {
				t.Errorf("SliceFind() got = %v, want %v", got, tt.want)
			}
			if got1 != tt.want1 {
				t.Errorf("SliceFind() got1 = %v, want %v", got1, tt.want1)
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
		want error
	}{
		{
			name: "delete an element value in the slice",
			args: args{
				slice: &([]int{10, 20, 50, 40, 30, 60}),
				i:     3,
			},
			want: nil,
		},
		{
			name: "delete an element value in the slice with different element type",
			args: args{
				slice: &([]int{10, 20, 50, 40, 30, 60}),
				i:     0,
			},
			want: nil,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			t.Log("Before", tt.args.slice)
			got := SliceDelete(tt.args.slice, tt.args.i)
			t.Log("After", tt.args.slice)
			if got != tt.want {
				t.Errorf("SliceDelete() got = %v, want %v", got, tt.want)
			}
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
		want error
	}{
		{
			name: "Insert an element value in the slice",
			args: args{
				slice: &([]int{10, 20, 50, 40, 30, 60}),
				i:     3,
				val:   70,
			},
			want: nil,
		},
		{
			name: "Insert an element value in the slice with different element type",
			args: args{
				slice: &([]int{10, 20, 50, 40, 30, 60}),
				i:     0,
				val:   "70",
			},
			want: nil,
		},
		{
			name: "Insert an element with -1 index",
			args: args{
				slice: &([]int{10, 20, 50, 40, 30, 60}),
				i:     -1,
				val:   "100",
			},
			want: fmt.Errorf("invalid index"),
		},
		{
			name: "Insert an element with 0 index",
			args: args{
				slice: &([]int{10, 20, 50, 40, 30, 60}),
				i:     0,
				val:   "100",
			},
			want: nil,
		},
		{
			name: "Insert an element with Len index",
			args: args{
				slice: &([]int{10, 20, 50, 40, 30, 60}),
				i:     6,
				val:   "100",
			},
			want: nil,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			t.Log("Before", tt.args.slice)
			got := SliceInsert(tt.args.slice, tt.args.i, tt.args.val)
			t.Log("After", tt.args.slice)
			if got != tt.want {
				if got == nil || tt.want == nil || got.Error() != tt.want.Error() {
					t.Errorf("SliceInsert() got = %v, want %v", got, tt.want)
				}
			}
		})
	}
}
