/*
 * File: misc.c header
 * Description: Contains helpere functions, and everything that doesn't fit elsewhere
 */
#include <stdint.h>
#ifndef MISC_H
#define MISC_H

#define ICON_COLUMN_WIDTH 20

/***************************
from catdoc and me ;-)
***************************/
#define fDot 0x0001   
#define fGlsy 0x0002
#define fComplex 0x0004
#define fPictures 0x0008 
#define fEncrypted 0x100
#define fReadOnly 0x400
#define fReserved 0x800
#define fExtChar 0x1000
#define fWord6 0xA5CC
#define fWord95 0xA5DC
#define fWord97 0xA5EC
#define iUnknownFile 0
#define iOldMSWordFile 1
#define iOleMsWordFile 2
#define iXmlMsWordFile 3
#define iMsWriteFile 4
#define iXmlOdtFile 5
#define iRtfFile 6
#define iPdfFile 7
#define iAbiwordFile 8
#define iXmlOdpFile 9
#define iXmlOdsFile 10
#define iXmlMsXlsFile 11
#define iXmlMsPptFile 12
/* for code pages conversions */
#define iCpUtf16 1
#define iCpW1252 2
#define iCpIbm437 3
#define iCpIso8859_1 4

extern GtkWidget *mainWindowApp; /* Holds pointer to the main searchmonkey GUI. Declared in main.c */

void miscErrorDialog(GtkWidget *widget, const gchar* msg);
gchar *misc_combo_index_to_size_units(gint index);

gint convertRegexGtk(size_t offset, const gchar *contents);
gchar *getFullFileName(GtkTreeView *treeView, gint columnNumber);
void initComboBox2(GtkWidget *comboBox);
void initComboBox(GtkWidget *comboBox);
void clearComboBox(GtkWidget *comboBox);
gboolean addUniqueRow(GtkWidget *comboBox, const gchar *entry);
gint g_strlen(const gchar *string);
void setExpertSearchMode (GtkWidget *widget, gboolean expertMode);
gboolean getExpertSearchMode (GtkWidget *widget);
void setResultsViewHorizontal(GtkWidget *widget, gboolean horizontal);
gboolean getResultsViewHorizontal (GtkWidget *widget);
void changeModel(GtkWidget *widget, const gchar *from, const gchar *to);
gchar * getDate(const gchar *curDate, GtkWidget *win);
gboolean test_regexp(gchar *regexp, guint flags, gchar *error);
gboolean validate_folder(const gchar *folderName);
uint16_t getshort(unsigned char *buffer,int offset);
int32_t getlong(unsigned char *buffer,int offset);
uint32_t getulong(unsigned char *buffer,int offset);
gint get_file_type_by_signature(gchar *path_to_file);
void misc_close_file(FILE *outputFile);
#endif /* MISC_H */
