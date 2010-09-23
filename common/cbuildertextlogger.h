#ifndef CBUILDERTEXTLOGGER_H
#define CBUILDERTEXTLOGGER_H

#include <QPlainTextEdit>

class CTextLogger
{
public:
    CTextLogger();
    virtual void setTextEditControl(QPlainTextEdit *control) { m_textEdit = control; }
    virtual void clear();
    virtual void write(QString text,bool updateNow=false);
    virtual void update();

private:
    QPlainTextEdit *m_textEdit;
    QString         m_text;
};

extern CTextLogger generalTextLogger;
extern CTextLogger buildTextLogger;
extern CTextLogger debugTextLogger;

#endif // CBUILDERTEXTLOGGER_H
