/*
 * Phomo.cs
 * Create photomosaics from F-Spot using Phomo
 *
 * Author(s)
 * 	Peter Goetz
 *
 * This is free software. See COPYING for details
 *
 */

using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;
using Gtk;

using FSpot;
using FSpot.Extensions;
using FSpot.Widgets;
using FSpot.Filters;
using FSpot.UI.Dialog;
using FSpot.Utils;
using Mono.Unix;
using System.Globalization;

using System.Xml;
using System.Xml.Serialization;


namespace PhomoExtension {

	public struct Configuration {
		public bool create_new_db_radio_button;
		public bool tags_radio_button;
		public ArrayList tags;
		public string	aspect_ratio_x;
		public string aspect_ratio_y;
		public bool save_for_later_checkbox;
		public string save_for_later_filename;
		public string	save_for_later_folder_chooser;
		public bool use_existing_db_button;
		public string use_existing_file_chooser;
		public int	x_res_in_pixels;
		public int x_res_in_stones;
		public int min_distance;
	}
	
	public class Phomo: ICommand
	{
		class ExtensionException: Exception {
			public ExtensionException(string message) : base(message) {}
		}
		
		[Glade.Widget] Gtk.Dialog dialog;
		[Glade.Widget] Gtk.RadioButton create_new_db_radio_button;
		[Glade.Widget] Gtk.VBox create_new_database_suboptions;
		[Glade.Widget] Gtk.RadioButton tags_radio_button;
		[Glade.Widget] Gtk.HBox tags_hbox;
		FSpot.Widgets.TagEntry tag_entry;
		[Glade.Widget] Gtk.Entry aspect_ratio_x;
		[Glade.Widget] Gtk.Entry aspect_ratio_y;
		[Glade.Widget] Gtk.CheckButton save_for_later_checkbox;
		[Glade.Widget] Gtk.HBox save_for_later_hbox;
		[Glade.Widget] Gtk.Entry save_for_later_filename;
		[Glade.Widget] Gtk.FileChooserButton save_for_later_folder_chooser;
		[Glade.Widget] Gtk.FileChooserButton use_existing_file_chooser;
		[Glade.Widget] Gtk.RadioButton use_existing_db_button;
		[Glade.Widget] Gtk.SpinButton x_res_in_stones;
		[Glade.Widget] Gtk.SpinButton x_res_in_pixels;
		[Glade.Widget] Gtk.SpinButton min_distance;
		
		FSpot.UI.Dialog.ThreadProgressDialog progress_dialog;
		
		
		const string PHOMO_PATH = "phomo";
		
		public void Run (object o, EventArgs e) {
			Log.Information ("Executing Phomo extension");
			if (MainWindow.Toplevel.SelectedPhotos ().Length == 0) {
				InfoDialog (Catalog.GetString ("No selection available"),
					    Catalog.GetString ("This tool requires an active selection. Please select one or more pictures and try again"),
					    Gtk.MessageType.Error);
				return;
			}
			if (!IsBackendInstalled()) {
				InfoDialog(Catalog.GetString ("Phomo not available"),
					   Catalog.GetString ("The phomo executable was not found in then path. Please check that you have it installed and that you have permissions to execute it"),
					   Gtk.MessageType.Error);
				return;
			}
			ShowDialog ();
		}

		private bool IsBackendInstalled() {
			string output = "";
			try {
				System.Diagnostics.Process mp_check = new System.Diagnostics.Process();
				mp_check.StartInfo.RedirectStandardOutput = true;
				mp_check.StartInfo.UseShellExecute = false;
				mp_check.StartInfo.FileName = "phomo";
				mp_check.StartInfo.Arguments = "--version";
				mp_check.Start();
				mp_check.WaitForExit ();
				StreamReader sroutput = mp_check.StandardOutput;
				output = sroutput.ReadLine();
			} catch (System.Exception) {
			}
			return System.Text.RegularExpressions.Regex.IsMatch(output, "^phomo");
		}

		public void ShowDialog () {
			Glade.XML xml = new Glade.XML (null, "Phomo.glade", "dialog", "f-spot");
			if(xml == null) return;
			xml.Autoconnect (this);
			dialog.Modal = false;
			dialog.TransientFor = null;

			tag_entry = new FSpot.Widgets.TagEntry (MainWindow.Toplevel.Database.Tags, false);
			tags_hbox.Add(tag_entry);

			dialog.Response += on_dialog_response;
			create_new_db_radio_button.Toggled += on_database_toggle;
			save_for_later_checkbox.Toggled += on_save_for_later_toggle;
			tags_radio_button.Toggled += on_tags_toggle;

			
			init_controls();

			dialog.ShowAll ();
		}
		
		void init_controls() {
			try {
				using (Stream stream = File.Open(config_file, FileMode.Open)) {
					var serializer = new XmlSerializer(typeof(Configuration));
					var config = (Configuration)serializer.Deserialize(stream);
					create_new_db_radio_button.Active = config.create_new_db_radio_button;
					tags_radio_button.Active = config.tags_radio_button;
					tag_entry.UpdateFromTagNames((string[])config.tags.ToArray(typeof(string)));
					aspect_ratio_x.Text = config.aspect_ratio_x.ToString();
					aspect_ratio_y.Text = config.aspect_ratio_y.ToString();
					save_for_later_checkbox.Active = config.save_for_later_checkbox;
					save_for_later_filename.Text = config.save_for_later_filename;
					save_for_later_folder_chooser.SetFilename(config.save_for_later_folder_chooser);
					use_existing_db_button.Active = config.use_existing_db_button;
					use_existing_file_chooser.SetFilename(config.use_existing_file_chooser);
					x_res_in_pixels.Value = System.Convert.ToDouble(config.x_res_in_pixels);
					x_res_in_stones.Value = System.Convert.ToDouble(config.x_res_in_stones);
					min_distance.Value = System.Convert.ToDouble(config.min_distance);
				}
			} catch {
				System.Console.WriteLine("Could not open Phomo config file. Using defaults.");
			}

		}
		
		private void on_database_toggle(object obj, EventArgs args) {
			use_existing_file_chooser.Sensitive = !create_new_db_radio_button.Active;
			create_new_database_suboptions.Sensitive = create_new_db_radio_button.Active;
		}

		private void on_tags_toggle(object obj, EventArgs args) {
			tag_entry.Sensitive = tags_radio_button.Active;
		}

		private void on_save_for_later_toggle(object obj, EventArgs args) {
			save_for_later_folder_chooser.Sensitive = save_for_later_checkbox.Active;
			save_for_later_filename.Sensitive = save_for_later_checkbox.Active;
			save_for_later_hbox.Sensitive = save_for_later_checkbox.Active;
		}

		Configuration config;
		void on_dialog_response (object obj, ResponseArgs args) {
			if (args.ResponseId == ResponseType.Ok) {
				config = save_controls();
				System.Threading.Thread command_thread = new System.Threading.Thread (createMosaics);
				command_thread.Name = Catalog.GetString ("Creating Photomosaic");
				progress_dialog = new FSpot.UI.Dialog.ThreadProgressDialog (command_thread, 1);
				progress_dialog.Response += HandleProgressAbort;
				progress_dialog.Start ();
			}
			dialog.Destroy ();
		}

		string config_file = Path.Combine(Environment.GetFolderPath (Environment.SpecialFolder.ApplicationData), Path.Combine("f-spot", "phomo.config"));
		Configuration save_controls() {
			var config = new Configuration();
			config.create_new_db_radio_button = create_new_db_radio_button.Active;
			config.tags_radio_button = tags_radio_button.Active;
			config.tags = new ArrayList(tag_entry.GetTypedTagNames());
			config.aspect_ratio_x = aspect_ratio_x.Text;
			config.aspect_ratio_y = aspect_ratio_y.Text;
			config.save_for_later_checkbox = save_for_later_checkbox.Active;
			config.save_for_later_filename = save_for_later_filename.Text;
			config.save_for_later_folder_chooser = save_for_later_folder_chooser.Filename;
			config.use_existing_db_button = use_existing_db_button.Active;
			config.use_existing_file_chooser = use_existing_file_chooser.Filename;
			config.x_res_in_pixels = x_res_in_pixels.ValueAsInt;
			config.x_res_in_stones = x_res_in_stones.ValueAsInt;
			config.min_distance = min_distance.ValueAsInt;
			
			try {
				using (Stream stream = File.Open(config_file, FileMode.Create))
				{
					var serializer = new XmlSerializer(typeof(Configuration));
					serializer.Serialize(stream, config);
				}
			} catch (Exception e) {
				Console.WriteLine(e);
			}
			return config;
		}
		
		void HandleProgressAbort(object sender, Gtk.ResponseArgs args)
		{
			deleteTempFile();
			System.Console.WriteLine("Phomo Aborted");
			process.Kill();
			process.WaitForExit();
		}
	
		
		string databaseFilename = "";
		public void createMosaics () {
			int error_count = -1;
			try {
				progress_dialog.Fraction = 0.0;
				if(config.create_new_db_radio_button) {
					if (config.save_for_later_checkbox)	{
						databaseFilename =  System.IO.Path.Combine(config.save_for_later_folder_chooser, config.save_for_later_filename);
					}
					else {
						databaseFilename = System.IO.Path.GetTempFileName();
					}
					buildDatabase(databaseFilename, aspect_ratio_x.Text + "x" + aspect_ratio_y.Text);
				} else {
					databaseFilename = config.use_existing_file_chooser;
				}
				progress_dialog.Fraction = 0.5;
				error_count = 0;
				double progress_slice = 0.5/MainWindow.Toplevel.SelectedPhotos ().Length;
				int i=0;
				foreach (Photo photo in MainWindow.Toplevel.SelectedPhotos ()) {
					try {
						
						string name = GetVersionName (photo);
						System.Uri outputUri = GetUriForVersionName (photo, name);
						render(databaseFilename, photo.DefaultVersionUri.LocalPath, outputUri.LocalPath, 
						       config.x_res_in_stones, config.x_res_in_pixels, config.min_distance, i*progress_slice, progress_slice);
						AddAndChangeToNewVersion(photo, outputUri, name);					
					} catch (Exception)	{
						error_count++;
					}
					i++;
				}		
			} catch (ExtensionException) {
				progress_dialog.ProgressText = Catalog.GetString ("Error. Could not finish.");
				progress_dialog.ButtonLabel = Gtk.Stock.Ok;
				progress_dialog.Response -= HandleProgressAbort;
			} finally {
				deleteTempFile();
				progress_dialog.Fraction = 1;
				if(error_count > -1) {
					if (error_count > 0) {
						progress_dialog.ProgressText = Catalog.GetString ("Finished with errors");
						progress_dialog.ButtonLabel = Gtk.Stock.Ok;
						progress_dialog.Response -= HandleProgressAbort;
					} else {
						Gtk.Application.Invoke (delegate { progress_dialog.Destroy(); });
					}
				}
			}
			
		}
		
		void deleteTempFile() {
			if (config.create_new_db_radio_button && !config.save_for_later_checkbox && databaseFilename != "") {
				try {
					System.IO.File.Delete(databaseFilename);
				} catch (System.Exception) {}
			}
		}

		void AddAndChangeToNewVersion(Photo p, System.Uri outputUri, string name) {
			p.DefaultVersionId = p.AddVersion (outputUri, name, true);
			p.Changes.DataChanged = true;
			MainWindow.Toplevel.Database.Photos.Commit (p);
		}
		System.Diagnostics.Process process;
		double step;
		double fraction;
		private void buildDatabase(string databaseFilename, string aspectRatio)
		{
			string arguments = "build-database --input-type file --photos-file - --database-filename " + 
				databaseFilename + " --aspect-ratio " + aspectRatio + " --raster-resolution 3";
			Console.WriteLine(PHOMO_PATH + " " + arguments);
			process = new System.Diagnostics.Process();
			process.StartInfo.RedirectStandardInput = true;
			process.StartInfo.RedirectStandardOutput = true;
			process.StartInfo.UseShellExecute = false;
			process.StartInfo.FileName = PHOMO_PATH;
			process.StartInfo.Arguments = arguments;
			process.OutputDataReceived += HandleOutput;
			process.Start();
			process.BeginOutputReadLine();
			StreamWriter inputStream = process.StandardInput;
			inputStream.AutoFlush = true;
			Photo [] photos = mosaicStones();
			step = 0.5/(double)(photos.Length);
			fraction = 0;
			foreach(Photo mosaicStone in photos) {
			    Console.WriteLine(mosaicStone.DefaultVersionUri.LocalPath);
				inputStream.WriteLine(mosaicStone.DefaultVersionUri.LocalPath);
			}
			Console.WriteLine();
			inputStream.WriteLine();
			inputStream.Close();
			process.WaitForExit();
			process.Close();
		}
		
		private void HandleOutput(object sendingProcess, 
            System.Diagnostics.DataReceivedEventArgs outLine)
        {
            if (!String.IsNullOrEmpty(outLine.Data))
            {
                Console.WriteLine(outLine.Data);
				if (outLine.Data.EndsWith("Added")) {
					fraction += step;
					progress_dialog.Fraction = fraction;
				}
            }
        }

		private Photo[] mosaicStones() {
			if (config.tags_radio_button) {
				Db db = MainWindow.Toplevel.Database;
				FSpot.PhotoQuery mini_query = new FSpot.PhotoQuery (db.Photos);
				mini_query.Terms = FSpot.OrTerm.FromTags (tags(db));
				Photo [] photos = mini_query.Photos;
				if(photos.Length == 0) {
					throw new ExtensionException("No photos in current selection.");
				}
				return photos;
			} else {
				return MainWindow.Toplevel.Query.Photos;
			}
		}
		
		Tag[] tags(Db db) {
			List<Tag> taglist = new List<Tag>();
			foreach (string tag_name in config.tags) {
				Tag t = db.Tags.GetTagByName (tag_name);
				if (t != null)
					taglist.Add(t);
			}
			return taglist.ToArray();
		}
		
		private void render(string databaseFilename, string picturePath, string outputFilename, 
		                           int xResInStones, int xResInPixels, int minDistance, double progress_slice_start, double progress_slice) {
			string arguments = "render --database-filename " + GLib.Shell.Quote(databaseFilename) + " --picture-path "
				+ GLib.Shell.Quote(picturePath) + " --output-filename " + GLib.Shell.Quote(outputFilename) +" --x-resolution-in-stones " + xResInStones +
					" --output-width " + xResInPixels + " --min-distance " + minDistance;
			Console.WriteLine(PHOMO_PATH +" "+arguments);

			process = new System.Diagnostics.Process();
			process.StartInfo.RedirectStandardOutput = true;
			process.StartInfo.UseShellExecute = false;
			process.StartInfo.FileName = PHOMO_PATH;
			process.StartInfo.Arguments = arguments;
			process.Start();
			StreamReader outputStream = process.StandardOutput;
			string line = outputStream.ReadLine();
			while(line != null) {
				if (line.EndsWith("%")) {
					double progress = Convert.ToDouble(double.Parse(line.Replace("%", ""), CultureInfo.InvariantCulture));
					progress_dialog.Fraction = 0.5 + progress_slice_start + progress_slice*progress/100;
				}
				line = outputStream.ReadLine();
			}

			process.WaitForExit();
			if (process.ExitCode != 0) {
				throw new Exception();
			}
			process.Close();
		}
	
		private string GetVersionName (Photo p)
		{
		    return GetVersionName (p, 1);
		}
		
		private string GetVersionName (Photo p, int i)
		{
	        string name = Catalog.GetPluralString ("PhotoMosaic", "PhotoMosaic ({0})", i);
	        name = String.Format (name, i);
	        if (p.VersionNameExists (name))
	                return GetVersionName (p, i + 1);
	        return name;
		}
		
		private System.Uri GetUriForVersionName (Photo p, string version_name)
		{
	        string name_without_ext = System.IO.Path.GetFileNameWithoutExtension (p.Name);
	        return new System.Uri (System.IO.Path.Combine (DirectoryPath (p),  name_without_ext
	                               + " (" + version_name + ")" + ".jpg"));
		}
		
        private static string DirectoryPath (Photo p)
        {
                System.Uri uri = p.VersionUri (Photo.OriginalVersionId);
                return uri.Scheme + "://" + uri.Host + System.IO.Path.GetDirectoryName (uri.AbsolutePath);
        }

		private void InfoDialog (string title, string msg, Gtk.MessageType type) {
			HigMessageDialog md = new HigMessageDialog (MainWindow.Toplevel.Window, DialogFlags.DestroyWithParent,
						  type, ButtonsType.Ok, title, msg);

			md.Run ();
			md.Destroy ();

		}

	}
}
