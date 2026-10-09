package internal

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"runtime"

	"github.com/charmbracelet/lipgloss"
)

var (
	SuccessStyle = lipgloss.NewStyle().
			Bold(true).
			Foreground(lipgloss.Color("42"))
	WarnStyle = lipgloss.NewStyle().
			Bold(true).
			Foreground(lipgloss.Color("214"))
	ErrorStyle = lipgloss.NewStyle().
			Bold(true).
			Foreground(lipgloss.Color("196"))
	DimText = lipgloss.NewStyle().
		Foreground(lipgloss.Color("244"))
	BoldText = lipgloss.NewStyle().Bold(true)
)

func Error(format string, a ...any) error {
	loc := ""
	if _, file, line, ok := runtime.Caller(1); ok {
		loc = fmt.Sprintf(" (%s:%d)", filepath.Base(file), line)
	}
	msg := fmt.Sprintf(format, a...)
	fmt.Fprintf(os.Stderr, "%s %s %s%s\n",
		ErrorStyle.Render("✗"),
		ErrorStyle.Render("ERROR"),
		msg,
		DimText.Render(loc),
	)
	return errors.New(msg)
}

func Warn(format string, a ...any) {
	msg := fmt.Sprintf(format, a...)
	fmt.Fprintf(os.Stderr, "%s %s  %s\n",
		WarnStyle.Render("▲"),
		WarnStyle.Render("WARN"),
		msg,
	)
}

func Log(format string, a ...any) {
	msg := fmt.Sprintf(format, a...)
	fmt.Fprintf(os.Stderr, "%s %s   %s\n",
		BoldText.Render("•"),
		DimText.Render("LOG"),
		msg,
	)
}

func Success(format string, a ...any) {
	msg := fmt.Sprintf(format, a...)
	fmt.Fprintf(os.Stderr, "%s %s\n",
		SuccessStyle.Render("[*]"),
		msg,
	)
}
