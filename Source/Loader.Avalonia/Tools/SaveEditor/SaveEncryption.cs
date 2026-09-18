using System;
using System.IO;
using System.Security.Cryptography;

namespace Loader.Tools.SaveEditor
{
    internal static class SaveEncryption
    {
        private static readonly byte[] ds3SaveKey = ParseHexString(
            "FD 46 4D 69 5E 69 A3 9A 10 E3 19 A7 AC E8 B7 FA");

        public static byte[] GetDS3SaveKey()
        {
            return (byte[])ds3SaveKey.Clone();
        }

        public static byte[] DecryptSL2File(byte[] encrypted, byte[] key)
        {
            byte[] iv = new byte[16];
            Buffer.BlockCopy(encrypted, 16, iv, 0, 16);

            using (Aes aes = Aes.Create())
            {
                aes.Mode = CipherMode.CBC;
                aes.BlockSize = 128;
                aes.Padding = PaddingMode.None;
                aes.Key = key;
                aes.IV = iv;

                ICryptoTransform decryptor = aes.CreateDecryptor();
                using (var encStream = new MemoryStream(encrypted, 32, encrypted.Length - 32))
                using (var cryptoStream = new CryptoStream(encStream, decryptor, CryptoStreamMode.Read))
                using (var decStream = new MemoryStream())
                {
                    cryptoStream.CopyTo(decStream);
                    return decStream.ToArray();
                }
            }
        }

        public static byte[] EncryptSL2File(byte[] decrypted, byte[] key)
        {
            using (Aes aes = Aes.Create())
            {
                aes.Mode = CipherMode.CBC;
                aes.BlockSize = 128;
                aes.Padding = PaddingMode.None;
                aes.Key = key;
                aes.GenerateIV();

                ICryptoTransform encryptor = aes.CreateEncryptor();
                using (var decStream = new MemoryStream(decrypted))
                using (var cryptoStream = new CryptoStream(decStream, encryptor, CryptoStreamMode.Read))
                using (var encStream = new MemoryStream())
                using (var md5 = MD5.Create())
                {
                    encStream.Write(aes.IV, 0, 16);
                    cryptoStream.CopyTo(encStream);
                    byte[] encrypted = new byte[encStream.Length + 16];
                    encStream.Position = 0;
                    encStream.Read(encrypted, 16, (int)encStream.Length);
                    byte[] hash = md5.ComputeHash(encrypted, 16, encrypted.Length - 16);
                    Buffer.BlockCopy(hash, 0, encrypted, 0, 16);
                    return encrypted;
                }
            }
        }

        private static byte[] ParseHexString(string hex)
        {
            string[] parts = hex.Split(' ');
            byte[] bytes = new byte[parts.Length];
            for (int i = 0; i < parts.Length; i++)
            {
                bytes[i] = Convert.ToByte(parts[i], 16);
            }
            return bytes;
        }
    }
}
