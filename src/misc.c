/*
 * File: misc.c
 * Description: Contains helpere functions, and everything that doesn't fit elsewhere
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "interface.h" /* glade requirement */
#include "support.h" /* glade requirement */
#include "misc.h"
#include <regex.h>

/*****************************************
 close file parsed with a correct trailer
******************************************/
void misc_close_file(FILE *outputFile)
{
  gchar end_sign[]={0x0a,0};
  fwrite(end_sign, sizeof(gchar),strlen(end_sign), outputFile); 
  fclose(outputFile);
}
/******************************************
  function to recognize the file type
  entry = path to file
  output = a gint code to switch
*****************************************/
gint get_file_type_by_signature(gchar *path_to_file)
{
  FILE *inputFile;
  gint retval = iUnknownFile;
  gchar *buffer;
  glong fileSize;
  gchar rtf_sign[]="{\\rtf";
  gchar start_sign[]={0x00, 0xD9, 0x00, 0x00, 0x00, 0x00};
  gchar old_word_sign[]={0xdb,0xa5,0};
  gchar write_sign[]={0x31,0xBE,0};
  gchar ole_sign[]={0xD0,0xCF,0x11,0xE0,0xA1,0xB1,0x1A,0xE1,0};
  gchar zip_sign[]="PK\003\004";
  gchar abiword_sign[]={0x3C,0x3F,0x78,0x6D,0};
  gchar pdf_sign[]={0x25, 0x50, 0x44, 0x46, 0};

  inputFile = fopen(path_to_file,"rb");
  if(inputFile==NULL) {
          printf("* ERROR : impossible to open file:%s to check signature *\n", path_to_file);
          return NULL;
  }
  /* we compute the size before dynamically allocate buffer */
   glong prev = ftell(inputFile);   
   fseek(inputFile, 0L, SEEK_END);
   glong sz = ftell(inputFile);
   fseek(inputFile, prev, SEEK_SET);
   if(sz>127)
     sz = 127;
   /* we allocate the buffer */
   buffer = g_malloc0(128);
   /* we start the file reading in Binary mode : it's better for future parsing */
   fileSize = fread(buffer, sizeof(gchar), sz, inputFile);
   fclose(inputFile);
   /* now we attempt to recognize signature */
   if (strncmp(buffer,&write_sign,2)==0)
      return iMsWriteFile;
   if (strncmp(buffer,&old_word_sign,2)==0)
      return iOldMSWordFile;
   if (strncmp(buffer,&ole_sign,8)==0)
      return iOleMsWordFile;
   if (strncmp(buffer,&rtf_sign,4)==0)
      return iRtfFile;
   if (strncmp(buffer,&pdf_sign,4)==0)
      return iPdfFile;
   if (strncmp(buffer, &zip_sign,4) == 0) {
     /* two cases : MS XML or Oasis XML */
     if(strncmp ((const gchar*)buffer+54,(const gchar *)"oasis", 5)==0) {
        /* now we switch between OASIS files signatures */
       if(strncmp ((const gchar*)buffer+73,(const gchar *)"textPK", 6)==0) {
          return iXmlOdtFile;}
       if(strncmp ((const gchar*)buffer+73,(const gchar *)"presen", 6)==0){
          return iXmlOdpFile;}
       if(strncmp ((const gchar*)buffer+73,(const gchar *)"spread", 6)==0){
          return iXmlOdsFile;}
       else
          return iUnknownFile;
     }
     else {/* dirty */
        /* it's a good Idea to switch between DOC-X, PPT-X and XLS-X */
        return iXmlMsWordFile;
     }
   }
   if (strncmp(buffer, &abiword_sign,4) == 0)
      if(strncmp ((const gchar*)buffer+0x31,(const gchar *)"abiwo", 5)==0)
           return iAbiwordFile;
   return retval;
}


/******************************************
 get a human readable string for size units
gint index :
0 = kb, 1 = Mb, 2 = Gb
********************************************/
gchar *misc_combo_index_to_size_units(gint index)
{
  gchar *str;

  switch(index)
   {
     case 0:
      {str=g_strdup_printf("%s",_("Kb"));break;}
     case 1:
      {str=g_strdup_printf("%s",_("Mb"));break;}
     case 2:
      {str=g_strdup_printf("%s",_("Gb"));break;}
     default:
      {str=g_strdup_printf("%s",_("N/A"));break;}
   }
 return str;
}

/****************************************************
  display an erreor/warning dialog
****************************************************/
void miscErrorDialog(GtkWidget *widget, const gchar* msg)
{ 
GtkWidget *dialog;

   dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(widget),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,"%s", 
                                           msg);
   gtk_dialog_run (GTK_DIALOG (dialog));
   gtk_widget_destroy (dialog);
}
/********************************************************************************************
  convert a regex byte position in a string to
  a gtk gchar compatible position - 
  Luc A janv 2018
  entry1 = offset means the offset computed by regex function, with multubytes chars
  entre2 = contents = the string returned by regex
  output = the "true" gchar position compatible with Gtk   
*******************************************************************************************/
gint convertRegexGtk(size_t offset, const gchar *contents)
{
 gint count = 0;
 gint len =0;
 gint i=0;
 size_t max = offset;

 while(max>0)
   {
      len = mblen(contents, max);
      if (len<1) break;
      //printf(">>> %c  <<<<", contents[i]);
      i++;
      contents+=len; 
      max-=len;
   }
 count = i;
 return count;
}


/*
 * callback/internal helper: returns the text stored within supplied columnNumber
 * Return string must be free'd.
 * TODO: convert column number to enumerator for table view results.
 * TODO: Consider renaming function to be more specific
 */
gchar *getFullFileName(GtkTreeView *treeView, gint columnNumber)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection(treeView); /* get selection */
  gchar *myString;
  GtkTreeIter iter;
  GtkTreeModel *model;

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    g_assert (model != NULL);
    gtk_tree_model_get (model, &iter,  columnNumber, &myString, -1);
    return myString;
  }
  return NULL;
}


/* Initializes Combo Box having two models */
void initComboBox2(GtkWidget *comboBox)
{
  GtkListStore* store;
  gint* setActive;


  store = gtk_list_store_new (1, G_TYPE_STRING); /* Create a simple 1-wide column */
  g_object_set_data(G_OBJECT(comboBox), "noregex", store);
  setActive = (gint *)g_malloc(sizeof(gint));
  *setActive = -1;
  g_object_set_data_full(G_OBJECT(comboBox), "noregex-active", (gpointer)setActive, &g_free);
  store = gtk_list_store_new (1, G_TYPE_STRING); /* Create a simple 1-wide column */
  g_object_set_data(G_OBJECT(comboBox), "regex", store);
  setActive = (gint *)g_malloc(sizeof(gint));
  *setActive = -1;
  g_object_set_data_full(G_OBJECT(comboBox), "regex-active", (gpointer)setActive, &g_free);
  gtk_combo_box_set_model(GTK_COMBO_BOX(comboBox), GTK_TREE_MODEL(store));
}


/*
 * Callback helper: intialise generic combo box with new model, and single width column storage
 */
void initComboBox(GtkWidget *comboBox)
{
  GtkListStore *store = gtk_list_store_new (1, G_TYPE_STRING); /* Create a simple 1-wide column */
  gtk_combo_box_set_model (GTK_COMBO_BOX(comboBox), GTK_TREE_MODEL(store));
  g_object_unref(store);
}

/*
 * Callback helper: clear all text entries (except active one) from a combo box with two models.
 */
void clearComboBox2(GtkWidget *comboBox)
{
    GtkTreeIter iter;
    gchar *readString;
    GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX(comboBox)));
    GtkListStore *store2 = GTK_LIST_STORE(g_object_get_data(G_OBJECT(comboBox), "regex"));
    gchar * clearActive = "regex-active";
    gchar * zeroActive = "noregex-active";

    g_assert(store != NULL);
    g_assert(store2 != NULL);

    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX(comboBox), &iter)) {
        return;
    }
    
    gtk_tree_model_get (GTK_TREE_MODEL(store), &iter,
                        0, &readString,
                        -1);
    gtk_list_store_clear (GTK_LIST_STORE(store));
    if (readString != NULL) {
        gtk_list_store_prepend (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, readString,
                            -1);
        g_free(readString);
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX(comboBox), &iter);
    }

    if (store == store2) {
      clearActive = "noregex-active";
      zeroActive = "regex-active";
      store2 = GTK_LIST_STORE(g_object_get_data(G_OBJECT(comboBox), "noregex"));
      g_assert(store2 != NULL);
    }
    gtk_list_store_clear(store2);

    gint * storeActive = (gint *)g_malloc(sizeof(gint));
    *storeActive = -1;
    g_object_set_data_full(G_OBJECT(comboBox), clearActive, storeActive, g_free);

    storeActive = (gint *)g_malloc(sizeof(gint));
    *storeActive = 0;
    g_object_set_data_full(G_OBJECT(comboBox), zeroActive, storeActive, g_free);
}


/*
 * Callback helper: clear all text entries (except active one) from a generic combo box model.
 */
void clearComboBox(GtkWidget *comboBox)
{
    GtkTreeIter iter;
    gchar *readString;
    GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX(comboBox)));

    g_assert(store != NULL);

    if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX(comboBox), &iter)) {
        return;
    }
    
    gtk_tree_model_get (GTK_TREE_MODEL(store), &iter,
                        0, &readString,
                        -1);
    gtk_list_store_clear (GTK_LIST_STORE(store));
    if (readString != NULL) {
        gtk_list_store_prepend (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, readString,
                            -1);
        g_free(readString);
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX(comboBox), &iter);
    }
}


/*
 * Callback/internal helper: add a text entry to a combo box model, making sure that no duplicates exist
 * Duplicate entries cause existing entry to be re-displayed - transparant to users.
 * TODO: replace strcmp function with GTK equivalent for UTF-8 strings
 */
gboolean addUniqueRow(GtkWidget *comboBox, const gchar *entry)
{
    GtkTreeIter iter;
    gchar *readString;
    GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX(comboBox)));

    g_assert(store != NULL);

	/* Test for NULL/Empty using GTK API */
    if ((entry == NULL) || g_ascii_isspace(entry[0])) {
      return TRUE;
    }

    /* Find first, and then loop through all until duplicate found */
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(store), &iter)) {
        do {
            gtk_tree_model_get (GTK_TREE_MODEL(store), &iter,
                                0, &readString,
                                -1);
            if (strcmp(readString, entry) == 0) {
                g_free(readString);
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX(comboBox), &iter);
                return FALSE;
            }
            g_free(readString);    
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
    }

/* Else unique, so add a new row to the model */
    gtk_list_store_prepend (store, &iter);
    gtk_list_store_set (store, &iter,
                        0, entry,
                        -1);
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX(comboBox), &iter);
    return TRUE;
}


/*
 * Low-tech replacement for standard strlen library for counting string bytes, assuming NULL terminated.
 * Note that "g_utf8_strlen" should be used if *character* count is required, as UTF8 chars are often multibyte
 */
gint g_strlen(const gchar *string)
{
  gint i = 0;
  while (string[i] != '\0') {
    i++;
  }
  return i;
}




/*
 * Callback helper: Return TRUE if in expert mode
 */
gboolean getExpertSearchMode (GtkWidget *widget)
{
  GtkWidget *searchNotebook = lookup_widget(widget, "hboxSearchmonkey");
  return TRUE; /* since Feb 2018, only Expert mode !*/
}

void sel (GtkTreeModel *model, GtkTreePath *path,GtkTreeIter *iter, gpointer data)
{
 GtkTreeSelection *tmpTreeSelection = NULL;

 tmpTreeSelection = (GtkTreeSelection *)data;

if((tmpTreeSelection!=NULL) && (model!=NULL)  && (path!=NULL))
    gtk_tree_selection_select_path(tmpTreeSelection, path);

}

/*
 * Callback helper: Switch between horizontal and vertical results display
 */
void setResultsViewHorizontal(GtkWidget *widget, gboolean horizontal)
{
  GtkTreeSelection *treeview1 = gtk_tree_view_get_selection(GTK_TREE_VIEW(lookup_widget(widget, "treeview1")));
  GtkTreeSelection *treeview2 = gtk_tree_view_get_selection(GTK_TREE_VIEW(lookup_widget(widget, "treeview2")));

  if((treeview1!=NULL)&&(treeview2!=NULL))
   {
      /* we must chek if selection != NULL - Luc A feb 2018 */

     if (horizontal) {
       gtk_widget_show(lookup_widget(widget, "resultsHPane"));
       gtk_widget_hide(lookup_widget(widget, "resultsVPane"));
       gtk_tree_selection_selected_foreach(treeview2, &sel, treeview1);
    } else { /* Vertical - default */
            gtk_widget_hide(lookup_widget(widget, "resultsHPane"));
            gtk_widget_show(lookup_widget(widget, "resultsVPane"));
            gtk_tree_selection_selected_foreach(treeview1, &sel, treeview2);
           }
   }/* if treeview!=NULL*/
}


/*
 * Callback helper: Return TRUE if horizontal, else vertical display
 */
gboolean getResultsViewHorizontal (GtkWidget *widget)
{
  GtkWidget *menuitem = lookup_widget(widget, "horizontal_results1");
  return (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)));
}


/* 
 * Callback helper: change the model for filename combo boxes when user changes regexp/glob preference
 */
void changeModel(GtkWidget *widget, const gchar * from, const gchar * to)
{
  gint* storeActive;
  GtkComboBox *advancedCombo = GTK_COMBO_BOX(lookup_widget(widget, "fileName"));
  GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model(advancedCombo));
  GtkListStore *setStore;

  setStore = GTK_LIST_STORE(g_object_get_data(G_OBJECT(advancedCombo), to));
  if (store != setStore) {
    gchar * setActiveKey = g_strconcat(from, "-active", NULL);
    gchar * getActiveKey = g_strconcat(to, "-active", NULL);
   
    /* Change the model */
    /* Advanced tab */
    storeActive = (gint *)g_malloc(sizeof(gint));
    *storeActive = gtk_combo_box_get_active(advancedCombo);
    g_object_set_data_full(G_OBJECT(advancedCombo), setActiveKey, (gpointer)storeActive, &g_free);
    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child (GTK_BIN (advancedCombo))), "");
    gtk_combo_box_set_model(advancedCombo, GTK_TREE_MODEL(setStore));
    gtk_combo_box_set_active(advancedCombo, *((gint *)g_object_get_data(G_OBJECT(advancedCombo), getActiveKey)));
    g_free((gpointer)setActiveKey);
    g_free((gpointer)getActiveKey);
  }
}

/*Callback helper: Displays a calendar popup and returns selected date */
gchar *getDate(const gchar *curDate, GtkWidget *win)
{
  GtkWidget * calendarDialog = create_calendarDialog(win);
  GtkCalendar *calendar = GTK_CALENDAR(lookup_widget(calendarDialog, "calendar1"));
  gchar* result = g_strdup(curDate);
  guint year, month, day;
  GDate date;
  
  g_date_set_parse(&date, curDate);
    
  if (g_date_valid(&date)) {
    year = g_date_get_year(&date);
    month = g_date_get_month(&date) - 1; /*Glib has months from 1-12 while GtkCalendar uses 0 - 11 */
    day = g_date_get_day(&date);
    gtk_calendar_select_day(calendar, day);
    gtk_calendar_select_month(calendar, month, year);
  }
    
  if (gtk_dialog_run(GTK_DIALOG(calendarDialog)) == GTK_RESPONSE_OK) {

    gtk_calendar_get_date(calendar, &year, &month, &day);
    gtk_widget_destroy(calendarDialog);

    result = (gchar *)g_malloc(sizeof(gchar) * 24);/* improved from 12 to 24 - Luc A., 27 déc 2017 */
    g_date_strftime(result, 23, "%d %b %Y", g_date_new_dmy(day, month +1, year));/* modifiyed from 11 to 23 - year with 4 digits  Luc A., 27 déc 2017 */
  } else {
    gtk_widget_destroy(calendarDialog);
  }

  return result;
}


gchar *get_regerror(int errcode, regex_t *compiled)
{
  size_t length = regerror (errcode, compiled, NULL, 0);
  gchar *buffer = (gchar *)g_malloc (sizeof(gchar) * length);
  (void) regerror (errcode, compiled, buffer, length);
  return buffer;
}

gboolean test_regexp(gchar *regexp, guint flags, gchar *error)
{
  gint regerr;
  regex_t testRegEx;
  GtkWidget *dialog;
  extern GtkWidget * mainWindowApp;
  GObject *window1 = G_OBJECT(mainWindowApp);

  regerr = regcomp(&testRegEx, regexp, flags);
  if (regerr != 0) {
    gchar* errorString = get_regerror(regerr, &testRegEx);
    gchar * msg = g_strconcat(error, "\n", errorString, NULL);

    dialog = gtk_message_dialog_new (GTK_WINDOW(window1),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,"%s",
                                     msg);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_free(errorString);
    g_free(msg);
    regfree(&testRegEx);
    return FALSE;
  } else {
    regfree(&testRegEx);
    return TRUE;
  }
}

/*
 * Used within misc, and search.c to validate that look-in folder exists and is not blank.
 * Produces pop-up error messages, and returns FALSE if folder not found.
 */
gboolean validate_folder(const gchar *folderName) {
  GtkWindow *window1 = GTK_WINDOW(mainWindowApp);
  GtkWidget *dialog;

  /* Test starting folder exists */
  if (folderName == NULL) {
    dialog = gtk_message_dialog_new (window1,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     _("Error! Look In directory cannot be blank."));
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    return FALSE;
  }
  
  if (!g_file_test(folderName, G_FILE_TEST_IS_DIR)) {
    dialog = gtk_message_dialog_new (window1,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     _("Error! Look In directory is invalid."));
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    return FALSE;
  }
  return TRUE;
}

/********************************************************************/
/* Reads 2-byte LSB  int from buffer at given offset platfom-indepent
 * way
 *********************************************************************/ 
uint16_t getshort(unsigned char *buffer,int offset) {
	return (unsigned short int)buffer[offset]|((unsigned short int)buffer[offset+1]<<8);
}  
/********************************************************************/
/* Reads 4-byte LSB  int from buffer at given offset almost platfom-indepent
 * way
 *********************************************************************/ 
int32_t getlong(unsigned char *buffer,int offset) {
	return (long)buffer[offset]|((long)buffer[offset+1]<<8L)
		|((long)buffer[offset+2]<<16L)|((long)buffer[offset+3]<<24L);
}  

uint32_t getulong(unsigned char *buffer,int offset) {
	return (unsigned long)buffer[offset]|((unsigned long)buffer[offset+1]<<8L)
		|((unsigned long)buffer[offset+2]<<16L)|((unsigned long)buffer[offset+3]<<24L);
}  
