using SoulsFormats;
using System.IO;
using System.Text.RegularExpressions;

namespace Loader.Tools.SaveEditor
{
    public class DS2Save
    {
        public string Path { get; }

        public int SteamID
        {
            get => Menu.SteamID;
            set => Menu.SteamID = value;
        }

        public MenuFile Menu { get; }
        public SaveSlot[] Slots { get; }
        private byte[] Regulation { get; }
        private BND4 bnd;

        public DS2Save(string path)
        {
            Path = path;
            bnd = BND4.Read(path);
            if (bnd.Files.Count != 12)
                throw new NotSupportedException($"Unexpected number of files in save: {bnd.Files.Count}");

            for (int i = 0; i < 12; i++)
            {
                var file = bnd.Files[i];
                if (!Regex.IsMatch(file.Name, $"^USER_DATA{i:D3}$"))
                    throw new NotSupportedException($"Unexpected filename in save: {file.Name}");
            }

            Menu = new MenuFile(Decrypt(bnd.Files[10]));
            Regulation = Decrypt(bnd.Files[11]);
            Slots = new SaveSlot[10];
            for (int i = 0; i < 10; i++)
                Slots[i] = new SaveSlot(Menu.SlotData[i], Decrypt(bnd.Files[i]));
        }

        public void Write()
        {
            for (int i = 0; i < 10; i++)
            {
                var slot = Slots[i];
                slot?.WriteSteamID(SteamID);
                if (slot != null)
                {
                    bnd.Files[i].Bytes = Encrypt(slot.SlotData);
                    Menu.SlotData[i] = slot.MenuData;
                }
            }
            bnd.Files[10].Bytes = Encrypt(Menu.Write());
            bnd.Files[11].Bytes = Encrypt(Regulation);
            bnd.Write(Path);
        }

        public static void Backup(string path)
        {
            string backupPath = path + ".bak";
            if (File.Exists(backupPath))
                File.Delete(backupPath);
            File.Copy(path, backupPath);
        }

        private static byte[] Decrypt(BinderFile file)
        {
            return SFUtil.DecryptSL2File(file.Bytes, SFUtil.GetDS3SaveKey());
        }

        private static byte[] Encrypt(byte[] bytes)
        {
            return SFUtil.EncryptSL2File(bytes, SFUtil.GetDS3SaveKey());
        }
    }
}