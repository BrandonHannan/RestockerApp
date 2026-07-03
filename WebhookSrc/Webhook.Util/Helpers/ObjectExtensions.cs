using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data.Models;

namespace Webhook.Util.Helpers
{
    public static class ObjectExtensions
    {
        public static bool ArePropertiesEqual<T>(this T self, T to, params string[] ignoreProperties) where T : class
        {
            if (self == null && to == null) return true;
            if (self == null || to == null) return false;

            var type = typeof(T);
            var ignoreList = new List<string>(ignoreProperties);

            foreach (var pi in type.GetProperties(BindingFlags.Public | BindingFlags.Instance))
            {
                // Skip the property if it exists on BaseModel
                if (typeof(BaseModel).GetProperty(pi.Name) != null) continue;

                // Skip manually ignored properties (like your virtual ones)
                if (ignoreList.Contains(pi.Name)) continue;

                var selfValue = pi.GetValue(self, null);
                var toValue = pi.GetValue(to, null);

                if (selfValue != toValue && (selfValue == null || !selfValue.Equals(toValue)))
                {
                    return false;
                }
            }
            return true;
        }

        private static bool IsSimpleType(Type type)
        {
            // If the type is nullable (e.g., Guid?), unwrap it to the underlying type (Guid)
            var underlyingType = Nullable.GetUnderlyingType(type) ?? type;

            return underlyingType.IsPrimitive ||
                   underlyingType.IsEnum ||
                   underlyingType == typeof(string) ||
                   underlyingType == typeof(decimal) ||
                   underlyingType == typeof(DateTime) ||
                   underlyingType == typeof(DateTimeOffset) ||
                   underlyingType == typeof(TimeSpan) ||
                   underlyingType == typeof(Guid);
        }
    }
}
