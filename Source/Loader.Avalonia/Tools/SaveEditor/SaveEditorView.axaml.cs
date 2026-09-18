using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Loader.Tools.SaveEditor
{
    public partial class SaveEditorView : UserControl
    {
        private DarkSoulsSave? _sourceSave;
        private DarkSoulsSave? _destSave;
        private SaveSlot? _selectedSlot;
        private Border? _selectedSlotBorder;
        private bool _copyRight = true;
        private readonly List<BankEntry> _bank = new();

        private class BankEntry
        {
            public SaveSlot Slot { get; }
            public Border Card { get; }
            public BankEntry(SaveSlot slot, Border card)
            {
                Slot = slot;
                Card = card;
            }
        }

        public SaveEditorView()
        {
            InitializeComponent();
        }

        private async void SourceLoadButton_OnClick(object? sender, RoutedEventArgs e)
        {
            var file = await PickSaveFileAsync();
            if (file != null)
            {
                try
                {
                    _sourceSave = new DarkSoulsSave(file.Path.LocalPath);
                    SourcePathText.Text = file.Name;
                    RefreshSlots(SourceSlotsPanel, _sourceSave, isSource: true);
                }
                catch (System.Exception ex)
                {
                    SourcePathText.Text = $"Error: {ex.Message}";
                }
            }
        }

        private async void DestLoadButton_OnClick(object? sender, RoutedEventArgs e)
        {
            var file = await PickSaveFileAsync();
            if (file != null)
            {
                try
                {
                    _destSave = new DarkSoulsSave(file.Path.LocalPath);
                    DestPathText.Text = file.Name;
                    RefreshSlots(DestSlotsPanel, _destSave, isSource: false);
                }
                catch (System.Exception ex)
                {
                    DestPathText.Text = $"Error: {ex.Message}";
                }
            }
        }

        private void SourceSaveButton_OnClick(object? sender, RoutedEventArgs e)
        {
            if (_sourceSave == null) return;
            try
            {
                DarkSoulsSave.Backup(_sourceSave.Path);
                _sourceSave.Write();
                SourcePathText.Text = "Saved with backup.";
            }
            catch (System.Exception ex)
            {
                SourcePathText.Text = $"Save error: {ex.Message}";
            }
        }

        private void DestSaveButton_OnClick(object? sender, RoutedEventArgs e)
        {
            if (_destSave == null) return;
            try
            {
                DarkSoulsSave.Backup(_destSave.Path);
                _destSave.Write();
                DestPathText.Text = "Saved with backup.";
            }
            catch (System.Exception ex)
            {
                DestPathText.Text = $"Save error: {ex.Message}";
            }
        }

        private void SwapDirectionButton_OnClick(object? sender, RoutedEventArgs e)
        {
            _copyRight = !_copyRight;
            CopyRightButton.Content = _copyRight ? "→" : "←";
        }

        private async Task<IStorageFile?> PickSaveFileAsync()
        {
            var top = TopLevel.GetTopLevel(this);
            if (top == null) return null;
            var options = new FilePickerOpenOptions
            {
                Title = "Select Save File (DS2/DS3)",
                FileTypeFilter = new List<FilePickerFileType>
                {
                    new FilePickerFileType("Save File") { Patterns = new List<string> { "*.sl2", "*.rds" } },
                    new FilePickerFileType("All Files") { Patterns = new List<string> { "*.*" } }
                }
            };

            var files = await top.StorageProvider.OpenFilePickerAsync(options);
            return files.FirstOrDefault();
        }

        private void RefreshSlots(StackPanel panel, DarkSoulsSave save, bool isSource)
        {
            panel.Children.Clear();
            for (int i = 0; i < 10; i++)
            {
                if (!save.Menu.OccupiedSlots[i]) continue;
                var slot = save.Slots[i];
            var card = CreateSlotCard(slot, i, isSource);
            panel.Children.Add(card);
            }
        }

        private Border CreateSlotCard(SaveSlot slot, int index, bool isSource)
        {
            var card = new Border { Classes = { "slot-card" } };
            var grid = new Grid();
            grid.ColumnDefinitions = new ColumnDefinitions
            {
                new ColumnDefinition(GridLength.Star),
                new ColumnDefinition(GridLength.Auto)
            };

            var info = new StackPanel { Spacing = 4 };
            info.Children.Add(new TextBlock { Text = slot.CharName, Classes = { "slot-name" } });
            info.Children.Add(new TextBlock { Text = $"Level {slot.SoulLevel} | Slot {index}", Classes = { "slot-info" } });

            var timeSpan = System.TimeSpan.FromSeconds(slot.PlaytimeSeconds);
            info.Children.Add(new TextBlock
            {
                Text = $"Playtime: {timeSpan.Days}d {timeSpan.Hours}h {timeSpan.Minutes}m",
                Classes = { "slot-info" }
            });

            Grid.SetColumn(info, 0);
            grid.Children.Add(info);

            var actions = new StackPanel { Orientation = Avalonia.Layout.Orientation.Horizontal, Spacing = 8 };
            var selectBtn = new Button { Content = "Select", Classes = { "slot-action" } };
            selectBtn.Click += (s, e) => SelectSlot(slot, card, isSource);
            actions.Children.Add(selectBtn);

            var bankBtn = new Button { Content = "→ Bank", Classes = { "slot-action" } };
            bankBtn.Click += (s, e) => AddToBank(slot);
            actions.Children.Add(bankBtn);

            Grid.SetColumn(actions, 1);
            grid.Children.Add(actions);

            card.Child = grid;
            return card;
        }

        private void SelectSlot(SaveSlot slot, Border card, bool isSource)
        {
            if (_selectedSlotBorder != null)
            {
                _selectedSlotBorder.Classes.Remove("selected");
            }
            _selectedSlotBorder = card;
            card.Classes.Add("selected");
            _selectedSlot = slot;
        }

        private void AddToBank(SaveSlot slot)
        {
            var card = CreateBankCard(slot);
            BankPanel.Children.Add(card);
            _bank.Add(new BankEntry(slot, card));
        }

        private Border CreateBankCard(SaveSlot slot)
        {
            var card = new Border { Classes = { "slot-card" } };
            var grid = new Grid();
            grid.ColumnDefinitions = new ColumnDefinitions
            {
                new ColumnDefinition(GridLength.Star),
                new ColumnDefinition(GridLength.Auto)
            };

            var info = new StackPanel { Spacing = 2 };
            info.Children.Add(new TextBlock { Text = slot.CharName, Classes = { "slot-name" } });
            info.Children.Add(new TextBlock { Text = $"Level {slot.SoulLevel}", Classes = { "slot-info" } });

            Grid.SetColumn(info, 0);
            grid.Children.Add(info);

            var actions = new StackPanel { Orientation = Avalonia.Layout.Orientation.Horizontal, Spacing = 4 };

            var toSrcBtn = new Button { Content = "→ L", Classes = { "slot-action" } };
            toSrcBtn.Click += (s, e) => LoadBankSlotToSave(slot, _sourceSave, SourceSlotsPanel, true);
            actions.Children.Add(toSrcBtn);

            var toDestBtn = new Button { Content = "→ R", Classes = { "slot-action" } };
            toDestBtn.Click += (s, e) => LoadBankSlotToSave(slot, _destSave, DestSlotsPanel, false);
            actions.Children.Add(toDestBtn);

            Grid.SetColumn(actions, 1);
            grid.Children.Add(actions);

            card.Child = grid;
            return card;
        }

        private void LoadBankSlotToSave(SaveSlot slot, DarkSoulsSave? save, StackPanel panel, bool isSource)
        {
            if (save == null)
            {
                if (isSource)
                    SourcePathText.Text = "Load a save first";
                else
                    DestPathText.Text = "Load a save first";
                return;
            }

            var emptySlot = -1;
            for (int i = 0; i < 10; i++)
            {
                if (!save.Menu.OccupiedSlots[i])
                {
                    emptySlot = i;
                    break;
                }
            }

            if (emptySlot == -1)
            {
                if (isSource)
                    SourcePathText.Text = "No empty slots";
                else
                    DestPathText.Text = "No empty slots";
                return;
            }

            save.Slots[emptySlot] = slot;
            save.Menu.OccupiedSlots[emptySlot] = true;
            save.Menu.SlotData[emptySlot] = slot.MenuData;
            RefreshSlots(panel, save, isSource);
            if (isSource)
                SourcePathText.Text = $"Loaded '{slot.CharName}' to slot {emptySlot}";
            else
                DestPathText.Text = $"Loaded '{slot.CharName}' to slot {emptySlot}";
        }

        private void ClearBankButton_OnClick(object? sender, RoutedEventArgs e)
        {
            BankPanel.Children.Clear();
            _bank.Clear();
        }

        private void CopyRightButton_OnClick(object? sender, RoutedEventArgs e)
        {
            if (_selectedSlot == null) return;

            DarkSoulsSave? source, dest;
            StackPanel sourcePanel, destPanel;
            TextBlock destTextBlock;

            if (_copyRight)
            {
                source = _sourceSave;
                dest = _destSave;
                sourcePanel = SourceSlotsPanel;
                destPanel = DestSlotsPanel;
                destTextBlock = DestPathText;
            }
            else
            {
                source = _destSave;
                dest = _sourceSave;
                sourcePanel = DestSlotsPanel;
                destPanel = SourceSlotsPanel;
                destTextBlock = SourcePathText;
            }

            if (source == null || dest == null)
            {
                destTextBlock.Text = "Load both saves first";
                return;
            }

            var emptySlot = -1;
            for (int i = 0; i < 10; i++)
            {
                if (!dest.Menu.OccupiedSlots[i])
                {
                    emptySlot = i;
                    break;
                }
            }

            if (emptySlot == -1)
            {
                destTextBlock.Text = "No empty slots in destination save";
                return;
            }

            dest.Slots[emptySlot] = _selectedSlot;
            dest.Menu.OccupiedSlots[emptySlot] = true;
            dest.Menu.SlotData[emptySlot] = _selectedSlot.MenuData;
            RefreshSlots(destPanel, dest, !_copyRight);
            destTextBlock.Text = $"Copied '{_selectedSlot.CharName}' to slot {emptySlot}";
        }
    }
}